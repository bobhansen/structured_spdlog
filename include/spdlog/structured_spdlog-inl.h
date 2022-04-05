// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#ifndef SPDLOG_NO_STRUCTURED_SPDLOG

#ifndef STRUCTURED_SPDLOG_INL_H
#define STRUCTURED_SPDLOG_INL_H

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#    include <spdlog/structured_spdlog.h>
#endif

// TODO(opt): static field names on the stack
//    Rather than passing std::initializer_list<Field>, I intuit that we should be able to construct
//    templates such that log_ eventually gets passed a std::array<string_view,N> && names and
//    std::array<FieldValue,N> && values.  The names will in most cases be constexprs and can be built
//    entirely in the data segment, and the runtime will be copying only the value data in

namespace spdlog {
namespace details {

SPDLOG_INLINE void append_value(const Field &field, memory_buf_t &dest)
{
    switch(field.value_type) {
        case FieldValueType::STRING_VIEW: {
            auto *buf_ptr = field.string_view_.data();
            dest.append(buf_ptr, buf_ptr + field.string_view_.size());
            break;
        }
        case FieldValueType::SHORT:     details::fmt_helper::append_int(field.short_,     dest); break;
        case FieldValueType::USHORT:    details::fmt_helper::append_int(field.ushort_,    dest); break;
        case FieldValueType::INT:       details::fmt_helper::append_int(field.int_,       dest); break;
        case FieldValueType::UINT:      details::fmt_helper::append_int(field.uint_,      dest); break;
        case FieldValueType::LONG:      details::fmt_helper::append_int(field.long_,      dest); break;
        case FieldValueType::ULONG:     details::fmt_helper::append_int(field.ulong_,     dest); break;
        case FieldValueType::LONGLONG:  details::fmt_helper::append_int(field.longlong_,  dest); break;
        case FieldValueType::ULONGLONG: details::fmt_helper::append_int(field.ulonglong_, dest); break;

        case FieldValueType::BOOL:      details::fmt_helper::append_string_view(field.bool_?"true":"false", dest); break;

        case FieldValueType::CHAR:      dest.push_back(field.char_); break;
        case FieldValueType::UCHAR:     details::fmt_helper::append_int(field.uchar_, dest); break;
        case FieldValueType::WCHAR:     details::fmt_helper::append_string_view(std::to_string(field.wchar_),       dest); break;

        // TODO: optimize these to only have at most one reallocation
        case FieldValueType::FLOAT:      details::fmt_helper::append_string_view(std::to_string(field.float_),      dest); break;
        case FieldValueType::DOUBLE:     details::fmt_helper::append_string_view(std::to_string(field.double_),     dest); break;
        case FieldValueType::LONGDOUBLE: details::fmt_helper::append_string_view(std::to_string(field.longdouble_), dest); break;
    }
}

SPDLOG_INLINE std::string value_to_string(const Field &field)
{
    memory_buf_t buf;
    append_value(field, buf);
    return to_string(buf);
}


SPDLOG_INLINE context_data::context_data(std::shared_ptr<context_data> parent_fields, const Field * fields, size_t num_fields) :
            parent_fields_(parent_fields), fields_(fields, fields + num_fields)
{
    // Find total required storage
    size_t size = 0;
    for (auto &field: fields_) {
        size += field.name.size();
        if (field.value_type == FieldValueType::STRING_VIEW) {
            size += field.string_view_.size();
        }
    }
    buffers_.resize(size);

    // Copy any string views into buffers_ and point the string view to the new copy
    size_t offset = 0;
    for (auto &field: fields_) {
        size_t field_size = field.name.size();
        std::memcpy(&buffers_[offset], field.name.data(), field_size);
        field.name = string_view_t(&buffers_[offset], field_size);
        offset += field_size;

        if (field.value_type == FieldValueType::STRING_VIEW) {
            field_size = field.string_view_.size();
            std::memcpy(&buffers_[offset], field.string_view_.data(), field_size);
            field.string_view_ = string_view_t(&buffers_[offset], field_size);
            offset += field_size;
        }
    }

    assert(offset == buffers_.size());
}

SPDLOG_INLINE context_data::~context_data()
{
    buffers_.resize(0);
}

} // namespace details


//
// Contexts
//
SPDLOG_INLINE context::context(std::initializer_list<Field> fields)
{
    if (fields.size() > 0) {
        std::shared_ptr<details::context_data>& context_head = details::threadlocal_context_head();

        context_to_restore_ = context_head;

        auto new_context_head = std::make_shared<details::context_data>(context_head, fields.begin(), fields.size());
        context_head = new_context_head;
    } else {
        // Never store a link with zero fields; iterator++ needs to know that when it follows a traversal, it will
        //    be pointing to context_data with at least one field.
        // When this goes out of context, just put the same head back
        context_to_restore_ = details::threadlocal_context_head();
    }
}

SPDLOG_INLINE context::~context()
{
    details::threadlocal_context_head() = context_to_restore_;
}

SPDLOG_INLINE void context::reset(std::initializer_list<Field> fields)
{
    auto parent_ptr = context_to_restore_;

    if (fields.size() > 0) {
        auto new_context_head = std::make_shared<details::context_data>(parent_ptr, fields.begin(), fields.size());
        details::threadlocal_context_head() = new_context_head;
    } else {
        // Reset current state to the parent of the old state
        details::threadlocal_context_head() = parent_ptr;

        // Never store a link with zero fields; iterator++ needs to know that when it follows a traversal, it will
        //    be pointing to context_data with at least one field.
        // When this goes out of context, just put the same head back
        context_to_restore_ = parent_ptr;
    }
}


// context_iterators
SPDLOG_INLINE context_iterator& context_iterator::operator++()
{
    if (data_ && (++idx_ == data_->fields_.size())) {
        data_ = data_->parent_fields_.get(); // may be nullptr
        idx_ = 0;
    }
    return *this;
}

SPDLOG_INLINE context_iterator::reference context_iterator::operator*()
{
    return data_->fields_[idx_];
}
SPDLOG_INLINE context_iterator::pointer context_iterator::operator->()
{
    return &data_->fields_[idx_];
}

// snapshots
SPDLOG_INLINE details::context_snapshot snapshot_context_fields()
{
    return details::threadlocal_context_head();
}

SPDLOG_INLINE replacement_context::replacement_context(details::context_snapshot data, std::initializer_list<Field> fields) :
    old_context_fields_(details::threadlocal_context_head())
{
    if (fields.size() == 0) {
        details::threadlocal_context_head() = data;
    } else {
        auto new_context_head = std::make_shared<details::context_data>(data, fields.begin(), fields.size());
        details::threadlocal_context_head() = new_context_head;
    }
}

SPDLOG_INLINE replacement_context::~replacement_context()
{
    details::threadlocal_context_head() = old_context_fields_;
}


} // namespace spdlog

#endif // STRUCTURED_SPDLOG_INL_H

#endif // SPDLOG_NO_STRUCTURED_SPDLOG
