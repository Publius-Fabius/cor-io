#include "corio/error.h"
#include "corio/kernel_events.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <expected>

namespace corio
{
    /** Non-Blocking Pipe */
    template<typename message> struct nonblocking_pipe 
    {
        event_fd reader;
        event_fd writer;

        using message_type = message;

        /**
         * Close both ends of the pipe.
         */
        void close() {
            reader.close();
            writer.close();
        }

        /**
         * Construct a nonblocking_pipe.
         * THROWS: system_error
         */
        
        nonblocking_pipe() : 
            reader(event_fd::unique()),
            writer(event_fd::unique())
        {
            assert(sizeof(message) <= PIPE_BUF);

            int fds[2];

            if(pipe(fds) == -1) 
                throw system_error("::pipe");

            reader.assign(fds[0]);
            writer.assign(fds[1]);

            int flags = -1;

            if((flags = fcntl(*reader, F_GETFL)) == -1) 
                throw system_error("::fcntl");
            if(fcntl(*reader, F_SETFL, O_NONBLOCK | flags) == -1) 
                throw system_error("::fcntl");
            
            if((flags = fcntl(*writer, F_GETFL)) == -1) 
                throw system_error("::fcntl");
            if(fcntl(*writer, F_SETFL, O_NONBLOCK | flags) == -1) 
                throw system_error("::fcntl");
        }

        ~nonblocking_pipe() = default;

        /**
         * Write to the pipe. 
         * 
         * RETURNS: ERR_WANTW, ERR_OK
         * THROWS: runtime_error, system_error
         */
        int write(message &msg)
        {
            auto result = ::write(*writer, &msg, sizeof(message));

            if(result == -1) {
                if(errno == EWOULDBLOCK || errno == EAGAIN) 
                    return ERR_WANTW;
                else 
                    throw system_error("write to pipe error");
            }

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
            auto result = ::read(*reader, &msg, sizeof(message));

            if(result == -1) {
                if(errno == EWOULDBLOCK || errno == EAGAIN)
                    return ERR_WANTR;
                else 
                    throw system_error("read from pipe error");
            }

            if(result != sizeof(message)) 
                throw runtime_error("malformed read from pipe");

            return ERR_OK;
        }
    };
}

