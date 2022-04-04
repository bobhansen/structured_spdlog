// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#ifndef SPDLOG_NO_STRUCTURED_SPDLOG

#ifndef STRUCTURED_SPDLOG_H
#define STRUCTURED_SPDLOG_H

#pragma once

#include "spdlog/common.h"

#include <iterator>

namespace spdlog {


SPDLOG_API struct context_iterator
{
    /**
        For formatters, walk all of the current context fields for formatting and display.

        if (msg.context_field_data) {
            for (auto &field: *msg.context_field_data) {
                std::cout << to_string(field.name) << "=" << spdlog::details::value_to_string(field) << " ";
            }
        }
    **/
    using iterator_category = std::forward_iterator_tag;
    using value_type        = Field;
    using pointer           = const Field*;
    using reference         = const Field&;

    context_iterator(details::context_data* ctx, size_t idx) :
        data_(ctx), idx_(idx) {}

    reference operator*();
    pointer operator->();

    // Prefix increment
    context_iterator& operator++();

    // Postfix increment
    context_iterator operator++(int) { context_iterator result(data_,idx_); ++result; return result; }

    friend bool operator== (const context_iterator& a, const context_iterator& b) { return a.data_ == b.data_ && a.idx_ == b.idx_; };
    friend bool operator!= (const context_iterator& a, const context_iterator& b) { return a.data_ != b.data_ || a.idx_ != b.idx_; };

private:
    details::context_data* data_;
    size_t                 idx_;
};

namespace details {
    void SPDLOG_API append_value(const Field &field, memory_buf_t &dest);
    std::string SPDLOG_API value_to_string(const Field &field);

    // A linked list of context_data field collections
    // TODO(opt): implement caching flattened parents
    //    We can track how many times there has been another context that
    //    points to this one as a parent.  As an optimization, we can accumulate all of
    //    the fields of the parents into fields_ when the number of children gets to 2 or 3
    //    NB: If number of children gets to (very large), reset it to (something small) to guard
    //    against wraparound at MAX_INT
    // TODO(opt): lazily use shared_ptr
    //    If we're not submitting to a multithreaded logger or snapshotting, we can just use raw pointers
    //    instead of taking the cache miss of an atomic operation in the shared_ptr.  We should
    //    benchmark to see how much it costs us in the single-threaded case
    SPDLOG_API struct context_data {
        std::shared_ptr<context_data> parent_fields_;
        std::vector<Field>            fields_;
        memory_buf_t                  buffers_; // storage for strings in fields_

        context_data(std::shared_ptr<context_data> parent_fields, const Field * fields, size_t num_fields);
        ~context_data();

        // Iterator support
        context_iterator begin() { return context_iterator(this, 0); }
        context_iterator end()   { return context_iterator(nullptr, 0); }
    };

    // Thread-local context fields
    using context_snapshot = std::shared_ptr<context_data>;
}


/**
    Using context to represent the state of the application:

    NOTE: it is important that the context variable has a name, or it will go out of scope immediately.
        RIGHT: spdlog::context ctx({{"field",value}});
        WRONG: spdlog::context({{"field",value}});

    void bar() {
        spdlog::info({{"processing", "bar"}});                       // prints "program:contextfield_demo running:foo processing:bar"
        std::thread th([ctx_snapshot=snapshot_context_fields()] {    // copy the threadlocal context for use in another thread
            spdlog::replacement_context ctx(ctx_snapshot);           // Log with ctx_snapshot while ctx is still on the stack
            spdlog::info({{"function", "in_thread"}});               // prints "program:contextfield_demo running:foo function:in_thread"
        });
        th.join();
    }

    void foo() {
        spdlog::context ctx({{"running", "foo"}}); // set context on the stack; goes away after ctx goes out of scope
        bar();
    }

    int main() {
        // Text output
        spdlog::default_logger()->set_pattern("%v%V"); // or just use the default
        foo();

        // JSON output
        auto formatter = spdlog::json_formatter::make_unique({{"program", "contextfield_demo"}}); // process-wide context
        spdlog::default_logger()->set_formatter(formatter);
        foo();
    }
**/
class SPDLOG_API context final {
public:
    context(std::initializer_list<Field> fields);
    ~context();

    // Replaces the contents of a context already on the stack with new values.
    //    Useful for putting at the top of a processing loop to hold the context and
    //    only reset it when something changes
    void reset(std::initializer_list<Field> fields);
private:
    std::shared_ptr<details::context_data> context_to_restore_;
};

/**
    replacement_context replaces the current thread's entire context with a snapshot
    of context from another thread.  Useful when work is being passed via lambdas to
    threadpools for execution.

    Can also be constructed with additional context for this instance.  For example:
        spdlog::context ctx({{"outer", "val"}});
        std::thread th([ctx_snapshot=snapshot_context_fields()] {    // copy the threadlocal context for use in another thread
            spdlog::replacement_context ctx(ctx_snapshot,
                {{"thread_id"}, std::this_thread::get_id()}});       // Restore outer context with additional local context
            spdlog::info({{"function", "in_thread"}});               // prints "outer:val thread_id:169 function:in_thread"
        });
        th.join();

**/
SPDLOG_API details::context_snapshot snapshot_context_fields();
class SPDLOG_API replacement_context final {
public:
    // Temporarily replace the current thread-local context with a snapshot from another thread
    replacement_context(details::context_snapshot data) : replacement_context(data, {}) {};

    // Temporarily replace the current thread-local context with a snapshot from another thread
    //    and additional local context
    replacement_context(details::context_snapshot data, std::initializer_list<Field> fields);
    ~replacement_context();
private:
    std::shared_ptr<details::context_data> old_context_fields_;
};


} // namespace spdlog

#endif // STRUCTURED_SPDLOG_H
#endif // NO_STRUCTURED_SPDLOG


#ifdef SPDLOG_HEADER_ONLY
#    include "structured_spdlog-inl.h"
#endif
