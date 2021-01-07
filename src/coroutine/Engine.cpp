#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {


Engine::~Engine() {
    if (StackBottom) {
        delete[] std::get<0>(idle_ctx->Stack);
        delete idle_ctx;
    }

    while (alive) {
        context* tmp_ctx = alive;
        delete[] std::get<0>(alive->Stack);
        delete tmp_ctx;
        alive = alive->next;
    }

    while (blocked) {
        context* tmp_ctx = blocked;
        delete[] std::get<0>(blocked->Stack);
        delete tmp_ctx;
        blocked = blocked->next;
    }
}

void Engine::Store(context &ctx) {
    char addr;
    if (&addr > StackBottom) {
        ctx.Hight = &addr;
    } else {
        ctx.Low = &addr;
    }

    size_t memory_size = ctx.Hight - ctx.Low;
    if (std::get<1>(ctx.Stack) < memory_size ||
        std::get<1>(ctx.Stack) > memory_size * 2) {
        delete[] std::get<0>(ctx.Stack);
        std::get<1>(ctx.Stack) = memory_size;
        std::get<0>(ctx.Stack) = new char[memory_size];
    }

    memcpy(std::get<0>(ctx.Stack), ctx.Low, memory_size);
}

void Engine::Restore(context &ctx) {
    char addr;
    while (&addr >= ctx.Low && &addr <= ctx.Hight) {
        Restore(ctx);
    }

    size_t memory_size = ctx.Hight - ctx.Low;
    memcpy(ctx.Low, std::get<0>(ctx.Stack), memory_size);
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    context* ctx = alive;
    if (ctx && ctx == cur_routine) {
        ctx = ctx->next;
    }

    if (ctx) {
        sched(ctx);
    }
}

void Engine::sched(void *routine) {
    if (routine == nullptr || routine == cur_routine) {
        yield();
        return;
    }

    if (cur_routine) {
        if (cur_routine != idle_ctx &&
            setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    cur_routine = static_cast<context*>(routine);
    Restore(*cur_routine);
}

void Engine::block(void* coro) {
    context* coro_to_block = static_cast<context*>(coro);
    if (coro == nullptr) {
        coro_to_block = cur_routine;
    }

    delete_from_list(alive, coro_to_block);
    add_to_list(blocked, coro_to_block);

    if (coro_to_block == cur_routine) {
        Restore(*idle_ctx);
    }
}

void Engine::unblock(void* coro) {
    context* coro_to_unblock = static_cast<context*>(coro);
    if (coro_to_unblock == nullptr) {
        return;
    }

    delete_from_list(blocked, coro_to_unblock);
    add_to_list(alive, coro_to_unblock);
}

void Engine::delete_from_list(context* head, context* coro) {
    if (head == coro) {
        head = head->next;
    }

    if (coro->prev != nullptr) {
        coro->prev->next = coro->next;
    }

    if (coro->next != nullptr) {
        coro->next->prev = coro->prev;
    }
}

void Engine::add_to_list(context* head, context* coro) {
    if (head == nullptr) {
        head = coro;
        head->prev = nullptr;
        head->next = nullptr;

    } else {
        coro->prev = nullptr;
        head->prev = coro;
        coro->next = head;
        head = coro;
    }
}


} // namespace Coroutine
} // namespace Afina
