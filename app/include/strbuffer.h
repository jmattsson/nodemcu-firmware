#ifndef _STRBUFFER_H_
#define _STRBUFFER_H_

#include "c_types.h"
#include <stdlib.h>

/**
 * The string buffer (opaque) type.
 */
typedef struct strbuffer strbuffer_t;

/**
 * Allocates a new string buffer.
 * @param sz initial size of the string buffer, or 0 for a default value.
 * @returns the new string buffer, or null if memory allocation failed.
 */
strbuffer_t *strbuffer_create (size_t sz);

/**
 * Resets the string buffer to a clean state.
 * @param sb the string buffer
 */
void strbuffer_reset (strbuffer_t *sb);

/**
 * Frees all storage used by the string buffer
 * @param sb the string buffer
 */
void strbuffer_free (strbuffer_t *sb);

/**
 * Appends a formatted string to the string buffer.
 *
 * @param sb the string buffer
 * @param fmt the printf-like formatting string
 * @param ... elements to format
 * @returns true on success, false on formatting error or insufficient memory
 */
bool strbuffer_add (strbuffer_t *sb, const char *fmt, ...) __attribute__((format(printf, 2, 3)));


/**
 * Appends a fixed string to the string buffer.
 * @param sb the string buffer
 * @param str the string to append
 * @param len the number of bytes from @c str to append
 * @return true on success, false on insufficient memory
 */
bool strbuffer_append (strbuffer_t *sb, const char *str, size_t len);

/**
 * Extracts the resulting string.
 *
 * This string *may* have its contents manipulated, but it may not be extended.
 * The returned pointer is considered invalid after a call to strbuffer_free(),
 * strbuffer_add() or strbuffer_resize().
 *
 * @param sb the string buffer
 * @param len optional pointer to receive the string length
 * @returns the string pointer
 */
char *strbuffer_str (strbuffer_t *sb, size_t *len);

/**
 * Attempts to resize (grow or shrink* a strbuffer.
 *
 * Automatically called when needed by strbuffer_add().
 *
 * @param sb the string buffer
 * @param sz the new size
 * @return true if the resizing could be done, false if there was insufficient
 *   memory or the new size was smaller than the used size.
 */
bool strbuffer_resize (strbuffer_t *sb, size_t sz);

#endif
