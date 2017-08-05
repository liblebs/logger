/*
 * C Source File
 *
 * Author: daddinuz
 * email:  daddinuz@gmail.com
 * Date:   August 04, 2017 
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "sds/sds.h"
#include "logger_handlers.h"

/*
 * Stream Handler
 */
static Logger_Buffer_T Logger_Handler_streamHandlerPublishCallback(Logger_Handler_T handler, char *content) {
    assert(handler);
    assert(content);
    FILE *const file = Logger_Handler_getFile(handler);
    long bytes_written = fprintf(file, "%s", content);
    if (bytes_written < 0) {
        errno = EIO;
        bytes_written = 0;
    }
    if (strcmp("\0", (content + bytes_written)) != 0) {
        errno = EIO;
    }
    Logger_Handler_setBytesWritten(handler, Logger_Handler_getBytesWritten(handler) + bytes_written);
    return Logger_Buffer_new((size_t) bytes_written, content);
}

Logger_Handler_T Logger_Handler_newStreamHandler(FILE *stream, Logger_Level_T level, Logger_Formatter_T formatter) {
    assert(stream);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    assert(formatter);
    return Logger_Handler_new(stream, NULL, level, formatter, Logger_Handler_streamHandlerPublishCallback, NULL);
}

/*
 * File Handler
 */
static void Logger_Handler_fileHandlerDeleteContextCallback(Logger_Handler_T handler, void *context) {
    assert(handler);
    fclose(Logger_Handler_getFile(handler));
    free(context);
}

Logger_Handler_T Logger_Handler_newFileHandler(
        Logger_String_T filePath, Logger_Level_T level, Logger_Formatter_T formatter
) {
    assert(filePath);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    assert(formatter);
    FILE *file = fopen(filePath, "w");
    return file ?
           Logger_Handler_new(
                   file, NULL, level, formatter,
                   Logger_Handler_streamHandlerPublishCallback, Logger_Handler_fileHandlerDeleteContextCallback
           ) : NULL;
}

/*
 * Rotating File Handler
 */
struct Logger_Handler_rotatingHandlerContext {
    size_t BYTES_BEFORE_ROTATION;
    size_t bytesWritten;
    size_t rotationCounter;
    Logger_String_T filePath;
};

static Logger_Buffer_T Logger_Handler_rotatingHandlerPublishCallback(Logger_Handler_T handler, char *content) {
    assert(handler);
    assert(content);
    FILE *file = Logger_Handler_getFile(handler);
    struct Logger_Handler_rotatingHandlerContext *context = Logger_Handler_getContext(handler);
    if (context->bytesWritten >= context->BYTES_BEFORE_ROTATION) {
        context->rotationCounter++;
        sds realFilePath = sdscatprintf(sdsempty(), "%s.%zu", context->filePath, context->rotationCounter);
        if (!realFilePath) {
            errno = ENOMEM;
            return NULL;
        }
        file = freopen(realFilePath, "w", file);
        sdsfree(realFilePath);
        if (!file) {
            return NULL;
        }
        context->bytesWritten = 0;
    }
    Logger_Buffer_T buffer = Logger_Handler_streamHandlerPublishCallback(handler, content);
    if (!buffer) {
        errno = ENOMEM;
        return NULL;
    }
    context->bytesWritten += buffer->size;
    return buffer;
}

Logger_Handler_T Logger_Handler_newRotatingHandler(
        Logger_String_T filePath, Logger_Level_T level, Logger_Formatter_T formatter, size_t bytesBeforeRotation
) {
    assert(filePath);
    assert(LOGGER_LEVEL_DEBUG <= level && level <= LOGGER_LEVEL_FATAL);
    assert(formatter);
    const size_t rotationCounter = 0;
    sds realFilePath = sdscatprintf(sdsempty(), "%s.%zu", filePath, rotationCounter);
    if (!realFilePath) {
        errno = ENOMEM;
        return NULL;
    }
    FILE *file = fopen(realFilePath, "w");
    sdsfree(realFilePath);
    if (!file) {
        return NULL;
    }
    struct Logger_Handler_rotatingHandlerContext *context = malloc(sizeof(*context));
    if (!context) {
        errno = ENOMEM;
        return NULL;
    }
    context->filePath = filePath;
    context->bytesWritten = 0;
    context->rotationCounter = rotationCounter;
    context->BYTES_BEFORE_ROTATION = bytesBeforeRotation;
    return Logger_Handler_new(
            file, context, level, formatter,
            Logger_Handler_rotatingHandlerPublishCallback, Logger_Handler_fileHandlerDeleteContextCallback
    );
}
