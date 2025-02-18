#ifndef _cvi_json_strerror_override_h_
#define _cvi_json_strerror_override_h_

/**
 * @file
 * @brief Do not use, cvi_json-c internal, may be changed or removed at any time.
 */

#include "config.h"
#include <errno.h>

#include "cvi_json_object.h" /* for JSON_EXPORT */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

JSON_EXPORT char *cvi_json_c_strerror(int errno_in);

#ifndef STRERROR_OVERRIDE_IMPL
#define strerror cvi_json_c_strerror
#endif

#ifdef __cplusplus
}
#endif

#endif /* _cvi_json_strerror_override_h_ */
