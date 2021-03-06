/*
 * C Source File
 *
 * Author: daddinuz
 * email:  daddinuz@gmail.com
 * Date:   July 19, 2017
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "logger.h"

typedef struct Logger_HandlersList_T {
    Logger_Handler_T handler;
    struct Logger_HandlersList_T *next;
} *Logger_HandlersList_T;

static Logger_HandlersList_T Logger_HandlersList_new(Logger_Handler_T handler, Logger_HandlersList_T next) {
    assert(handler);
    Logger_HandlersList_T self = malloc(sizeof(*self));
    if (self) {
        self->handler = handler;
        self->next = next;
    }
    return self;
}

static void Logger_HandlersList_delete(Logger_HandlersList_T *ref) {
    assert(ref);
    assert(*ref);
    Logger_HandlersList_T self = *ref;
    free(self);
    *ref = NULL;
}

struct Logger_T {
    const char *name;
    Logger_Level_T level;
    Logger_HandlersList_T handlers;
};

Logger_T Logger_new(const char *name, Logger_Level_T level) {
    assert(name);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    Logger_T self = malloc(sizeof(*self));
    if (self) {
        self->name = name;
        self->level = level;
        self->handlers = NULL;
    }
    return self;
}

void Logger_delete(Logger_T *ref) {
    assert(ref);
    assert(*ref);
    Logger_T self = *ref;
    Logger_HandlersList_T current, next;
    for (current = self->handlers; current; current = next) {
        next = current->next;
        Logger_HandlersList_delete(&current);
    }
    free(self);
    *ref = NULL;
}

void Logger_deepDelete(Logger_T *ref) {
    assert(ref);
    assert(*ref);
    Logger_T self = *ref;
    for (Logger_Handler_T handler = Logger_popHandler(self); handler; handler = Logger_popHandler(self)) {
        Logger_Formatter_T formatter = Logger_Handler_getFormatter(handler);
        Logger_Formatter_delete(&formatter);
        Logger_Handler_delete(&handler);
    };
    Logger_delete(ref);
}

const char *Logger_getName(Logger_T self) {
    assert(self);
    return self->name;
}

Logger_Level_T Logger_getLevel(Logger_T self) {
    assert(self);
    return self->level;
}

Logger_Handler_T Logger_removeHandler(Logger_T self, Logger_Handler_T handler) {
    assert(self);
    assert(handler);
    Logger_Handler_T outHandler = NULL;
    Logger_HandlersList_T prev = NULL, base = self->handlers;
    while (base) {
        if (base->handler == handler) {
            if (prev) {
                prev->next = base->next;
            } else {
                self->handlers = base->next;
            }
            outHandler = base->handler;
            Logger_HandlersList_delete(&base);
            break;
        }
        prev = base;
        base = base->next;
    }
    return outHandler;
}

Logger_Handler_T Logger_popHandler(Logger_T self) {
    assert(self);
    Logger_Handler_T outHandler = NULL;
    Logger_HandlersList_T tmp = NULL;
    if (self->handlers) {
        tmp = self->handlers;
        outHandler = self->handlers->handler;
        self->handlers = self->handlers->next;
        Logger_HandlersList_delete(&tmp);
    }
    return outHandler;
}

void Logger_setName(Logger_T self, const char *name) {
    assert(self);
    assert(name);
    self->name = name;
}

void Logger_setLevel(Logger_T self, Logger_Level_T level) {
    assert(self);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    self->level = level;
}

Logger_Handler_T Logger_addHandler(Logger_T self, Logger_Handler_T handler) {
    assert(self);
    assert(handler);
    Logger_HandlersList_T head = Logger_HandlersList_new(handler, self->handlers);
    if (!head) {
        return NULL;
    }
    self->handlers = head;
    return handler;
}

bool Logger_isLoggable(Logger_T self, Logger_Level_T level) {
    assert(self);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    return level >= self->level;
}

Logger_Err_T Logger_logRecord(Logger_T self, Logger_Record_T record) {
    assert(self);
    assert(record);
    Logger_Err_T err = LOGGER_ERR_OK;
    if (Logger_isLoggable(self, Logger_Record_getLevel(record))) {
        for (Logger_HandlersList_T base = self->handlers; base; base = base->next) {
            if (Logger_Handler_isLoggable(base->handler, record)) {
                err = Logger_Handler_publish(base->handler, record);
                if (LOGGER_ERR_OK != err) {
                    break;
                }
            }
        }
    }
    return err;
}

Logger_Err_T _Logger_log(
        Logger_T self, Logger_Level_T level, const char *file, size_t line, const char *function, time_t timestamp,
        const char *fmt, ...
) {
    assert(self);
    assert(file);
    assert(function);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    assert(fmt);

    va_list args;
    Logger_Err_T err;
    Logger_String_T message = NULL;
    Logger_Record_T record = NULL;

    va_start(args, fmt);
    do {
        message = Logger_String_fromArgumentsList(fmt, args);
        if (!message) {
            err = LOGGER_ERR_OUT_OF_MEMORY;
            break;
        }

        record = Logger_Record_new(Logger_getName(self), level, file, line, function, timestamp, message);
        if (!record) {
            err = LOGGER_ERR_OUT_OF_MEMORY;
            break;
        }

        err = Logger_logRecord(self, record);
    } while (false);
    va_end(args);

    if (record) {
        Logger_Record_delete(&record);
    }
    if (message) {
        Logger_String_delete(&message);
    }
    return err;
}
