#ifndef CORIO_ERROR_H
#define CORIO_ERROR_H

#include <system_error>
#include <errno.h>
#include <stdexcept>

namespace corio
{
    static inline std::system_error system_error(const char *msg)
    {
        return std::system_error(errno, std::generic_category(), msg);
    }

    using runtime_error = std::runtime_error;

    /** Error Type */
    enum error_type 
    {
        ERR_OK      = 0,                        /** All OK */
        ERR_SYS     = -1,                       /** Syscall Error */

        /* PGENC Errors */

        // ERR_OOB     = PGC_ERR_OOB,          /** Out of Bounds Error */
        // ERR_CMP     = PGC_ERR_CMP,          /** Comparison Error */
        // ERR_ENC     = PGC_ERR_ENC,          /** Encoding Error  */
        // ERR_SYN     = PGC_ERR_SYN,          /** Syntax Error */
        // ERR_FLO     = PGC_ERR_FLO,          /** Numeric Over/Under Error */
        // ERR_OOM     = PGC_ERR_OOM,          /** Out of Memory Error */

        /* Control Errors */

        ERR_MODE    = -1700,                    /** Bad Mode */
        ERR_GAI     = -1702,                    /** GAI Error */
        ERR_WANTW   = -1703,                    /** Want to Write */
        ERR_WANTR   = -1704,                    /** Want to Read */
        ERR_TIME    = -1705,                    /** Timeout Error */
        ERR_LIMIT   = -1706,                    /** Limit Reached */
        ERR_EOF     = -1707,                    /** EOF Encountered */
        ERR_SSL     = -1708                     /** SSL Error */

        /* HTTP Errors */

        // LAW_ERR_SSL_ACCEPT_FAILED               = -1800,
        // LAW_ERR_SSL_SHUTDOWN_FAILED             = -1801,
        // LAW_ERR_REQLINE_TOO_LONG                = -1802,
        // LAW_ERR_HEADERS_TOO_LONG                = -1803,
        // LAW_ERR_REQLINE_MALFORMED               = -1804,
        // LAW_ERR_HEADERS_MALFORMED               = -1805,
        // LAW_ERR_REQLINE_TIMEOUT                 = -1806,
        // LAW_ERR_HEADERS_TIMEOUT                 = -1807,
        // LAW_ERR_SSL_SHUTDOWN_TIMEOUT            = -1808
    };

}

#endif