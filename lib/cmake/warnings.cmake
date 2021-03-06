# The MIT License (MIT)
#
# Copyright (c) 2016 Mateusz Pusz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


# Configure compiler warning level
macro(set_warnings)
    if(${ARGC} GREATER_EQUAL 1)
        if(${ARGV0} STREQUAL "TREAT_AS_ERRORS")
            set(treat_warnings_as_errors TRUE)
        else()
            message(FATAL_ERROR "Unknown argument '${ARGV0}' passed to set_warnings()")
        endif()
    endif()

    if(MSVC)
        # set warnings
        if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
            string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
        endif()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w44062 /w44263 /w44266 /w44640")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wv:18")

        # disable language extensions
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")

        add_compile_definitions("_CRT_SECURE_NO_WARNINGS")

        if(treat_warnings_as_errors)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        # set warnings
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat=2 -Wpedantic")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused -Wnon-virtual-dtor -Woverloaded-virtual")
#        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast -Wcast-qual -Wcast-align")
#        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wimplicit-fallthrough")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-redundant-move")

        include(CheckCXXCompilerFlag)

        check_cxx_compiler_flag(-Wlogical-op HAVE_WLOGICAL_OP)
        if(HAVE_WLOGICAL_OP)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wlogical-op")
        endif()
        check_cxx_compiler_flag(-Wduplicated-cond HAVE_WDUPLICATED_COND)
        if(HAVE_WDUPLICATED_COND)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wduplicated-cond")
        endif()
        check_cxx_compiler_flag(-Wduplicated-branches HAVE_WDUPLICATED_BRANCHES)
        if(HAVE_WDUPLICATED_BRANCHES)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wduplicated-branches")
        endif()

        if(treat_warnings_as_errors)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
        endif()
    else()
        message(NOTICE "Not adding compiler warnings due to unknown compiler")
    endif()
endmacro()
