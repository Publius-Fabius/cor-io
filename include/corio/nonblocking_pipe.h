
#include "corio/error.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <expected>

namespace corio
{
    /** Non-Blocking Pipe */
    template<typename message> class nonblocking_pipe 
    {
        int fds[2];

        public:

        using message_type = message;

        /**
         * Close both ends of the pipe.
         */
        void close()
        {
            if(fds[0] != -1) 
                if(::close(fds[0]) == -1)
                    throw system_error("::close");
            fds[0] = -1;
            if(fds[1] != -1) 
                if(::close(fds[1]) == -1)
                    throw system_error("::close");
            fds[1] = -1;
        }

        /**
         * Construct a nonblocking_pipe.
         * THROWS: system_error
         */
        nonblocking_pipe() : fds{-1, -1}
        {
            assert(sizeof(message) <= PIPE_BUF);

            if(pipe(fds) == -1) 
                throw system_error("::pipe");

            int flags = -1;

            if((flags = fcntl(fds[0], F_GETFL)) == -1) {
                close();
                throw system_error("::fcntl");
            }
            if(fcntl(fds[0], F_SETFL, O_NONBLOCK | flags) == -1) {
                close();
                throw system_error("::fcntl");
            }

            if((flags = fcntl(fds[1], F_GETFL)) == -1) {
                close();
                throw system_error("::fcntl");
            }
            if(fcntl(fds[1], F_SETFL, O_NONBLOCK | flags) == -1) {
                close();
                throw system_error("::fcntl");
            }
        }

        ~nonblocking_pipe()
        {
            close();
        }

        /**
         * Write to the pipe. 
         * 
         * RETURNS: ERR_WANTW, ERR_OK
         * THROWS: runtime_error, system_error
         */
        int write(message &msg)
        {
            const ssize_t result = ::write(fds[1], &msg, sizeof(message));

            if(result == -1) 
                if(errno == EWOULDBLOCK || errno == EAGAIN) 
                    return ERR_WANTW;
                else 
                    throw system_error("write to pipe error");

            if(result != sizeof(message_type)) 
                throw runtime_error("malformed write to pipe"); 

            return ERR_OK;
        }

        /**
         * Read from the pipe. 
         * 
         * RETURNS: ERR_WANTR, ERR_OK
         * THROWS: runtime_error, system_error
         */
        int read(message &msg)
        {
            const ssize_t result = ::read(fds[0], &msg, sizeof(message));

            if(result == -1) 
                if(errno == EWOULDBLOCK || errno == EAGAIN)
                    return ERR_WANTR;
                else 
                    throw system_error("read from pipe error");

            if(result != sizeof(message)) 
                throw runtime_error("malformed read from pipe");

            return ERR_OK;
        }
    };
}
