/**
 * \file mvsyscall.c
 * \brief Wrappers required to use newlib malloc-family within FreeRTOS.
 *        The syscalls for stdlib are implemented in a minimum way as well
 *
 * \par Overview
 * Route FreeRTOS memory management functions to newlib's malloc family.
 * Thus newlib and FreeRTOS share memory-management routines and memory pool,
 * and all newlib's internal memory-management requirements are supported.
 *
 * \author Dave Nadler
 * \author Thorsten Kimmel
 * \date 20-August-2019
 * \version 02-Dec-2021 implemented syscalls for stdlib
 * \version 27-Jun-2020 Correct "FreeRTOS.h" capitalization, commentary
 * \version 24-Jun-2020 commentary only
 * \version 11-Sep-2019 malloc accounting, comments, newlib version check
 *
 * \see http://www.nadler.com/embedded/newlibAndFreeRTOS.html
 * \see https://sourceware.org/newlib/libc.html#Reentrancy
 * \see https://sourceware.org/newlib/libc.html#malloc
 * \see https://sourceware.org/newlib/libc.html#index-_005f_005fenv_005flock
 * \see https://sourceware.org/newlib/libc.html#index-_005f_005fmalloc_005flock
 * \see https://sourceforge.net/p/freertos/feature-requests/72/
 * \see http://www.billgatliff.com/newlib.html
 * \see http://wiki.osdev.org/Porting_Newlib
 * \see http://www.embecosm.com/appnotes/ean9/ean9-howto-newlib-1.0.html
 *
 *
 * \copyright
 * (c) Dave Nadler 2017-2020, All Rights Reserved.
 * Web:         http://www.nadler.com
 * email:       drn@nadler.com
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Use or redistributions of source code must retain the above copyright notice,
 *   this list of conditions, and the following disclaimer.
 *
 * - Use or redistributions of source code must retain ALL ORIGINAL COMMENTS, AND
 *   ANY CHANGES MUST BE DOCUMENTED, INCLUDING:
 *   - Reason for change (purpose)
 *   - Functional change
 *   - Date and author contact
 *
 * - Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * Includes
 */
// ----------------------------------------------------------------------------
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <newlib.h>
#include <malloc.h>
#include <FreeRTOS.h>

/*!
 * In case there is a FreeRTOS used for this application, we have to include
 * additional dependencies which are used in memory wrappers
 */
// ----------------------------------------------------------------------------
#ifdef INC_FREERTOS_H
#  include "task.h"
#  include "semphr.h"
#endif
// ----------------------------------------------------------------------------

/*!
 * conditional check to make sure this syscall abstraction and the memory
 * wrapper fits to the correct newlib / stdlib version
 */
// ----------------------------------------------------------------------------
#if (( __NEWLIB__ != 3) && ( __NEWLIB__ != 4 )) || ((__NEWLIB__ != 3) && (__NEWLIB_MINOR__ < 1)) || \
    ((__NEWLIB__ == 3) && (__NEWLIB_MINOR__ > 3) )
#warning "This wrapper was verified for newlib versions 3.1 - 3.3; please ensure newlib's external requirements for malloc-family are unchanged!"
#endif
// ----------------------------------------------------------------------------

/*!
 * conditional check to make sure the correct reentrancy configuration has been
 * set. Otherwise the stdlib calls could be lead to undefined behavior whenever
 * a function is called simultaneous by more than one thread
 */
// ----------------------------------------------------------------------------
#ifdef INC_FREERTOS_H
#  if !defined(configUSE_NEWLIB_REENTRANT) ||  (configUSE_NEWLIB_REENTRANT != 1)
#     error "#define configUSE_NEWLIB_REENTRANT 1 which is required for thread-safety of newlib sprintf, dtoa, strtok, etc..."
#  endif
#endif
// ----------------------------------------------------------------------------

#ifdef INC_FREERTOS_H
// ----------------------------------------------------------------------------
// The following functions are used to lock IRQs to make sure the newlib functions
// can be used in an IRQ as well, this is not working as expected and so we disable it
// for our platform so far, therefore we have to make sure the user can not enter newlib
// functions when running in ISR context
//! Disables interrupts (after saving prior state)
#  define NEWLIB_FUNCS_INSIDE_ISR 0
#  if defined(NEWLIB_FUNCS_INSIDE_ISR) && NEWLIB_FUNCS_INSIDE_ISR != 0
define DRN_ENTER_CRITICAL_SECTION(_usis) { _usis = taskENTER_CRITICAL_FROM_ISR(); }
#  else
#    define DRN_ENTER_CRITICAL_SECTION(_usis) do { int insideAnISR = xPortIsInsideInterrupt(); configASSERT( !insideAnISR ); } while(0)
#  endif

//! Re-enables interrupts (unless already disabled prior taskENTER_CRITICAL)
#  if defined(NEWLIB_FUNCS_INSIDE_ISR) && NEWLIB_FUNCS_INSIDE_ISR != 0
#    define DRN_EXIT_CRITICAL_SECTION(_usis)  { taskEXIT_CRITICAL_FROM_ISR(_usis);     }
#  else
#    define DRN_EXIT_CRITICAL_SECTION(_usis)
#  endif

// ----------------------------------------------------------------------------
#  define __HeapBase  _end
#  define __HeapLimit _estack

#define MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_STDOUT
#define MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_MALLOC
#define MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_ENV

/*!
 * The symbols __HeapBase and __HeapLimit are defined to read the
 * heap parameters from symbols defined in the linker script. The
 * define macros above are used to adapt these names to the name
 * of the variable in the linker script. Both variables will be used
 * later to allocate and manage memory of the heap and to detect a
 * stack / heap crash...
 */
// ----------------------------------------------------------------------------
extern char __HeapBase, __HeapLimit;

int _write( int file, char* ptr, int len );
int _read( int file, char* ptr, int len );

/*!
 * The following semaphore is used to prevent the stdout and stderr device
 * to be accessed simultaneous while writing data. This is necessary because
 * the stdlib itself is thread safe and has its own buffers for a thread but
 * the access to the hardware must be locked as well
 */
// ----------------------------------------------------------------------------
#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_STDOUT)
static SemaphoreHandle_t stdioSemaphore;
#endif
#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_MALLOC)
static SemaphoreHandle_t mallocSemaphore;
#endif
#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_ENV)
static SemaphoreHandle_t envSemaphore;
#endif
#endif


/*!
 *  pointer to array of char* strings that define the current
 *  environment variables of the applications environment
 */
// ----------------------------------------------------------------------------
#define xstr(a) str(a)
#define str(a) #a
#define NEWLIB_ENV "STDLIB=newlib-" xstr(__NEWLIB__) "." xstr(__NEWLIB_MINOR__)
#ifdef NDEBUG
#  define DEBUG_ENV "DEBUG_ENV=RELEASE"
#else
#  define DEBUG_ENV "DEBUG_ENV=DEBUG"
#endif
#define GNUC_ENV "GNUC=" xstr(__GNUC__) "." xstr(__GNUC_MINOR__)
#define OS_VERSION_ENV "OSVERSION=" xstr(tskKERNEL_VERSION_MAJOR) "." xstr(tskKERNEL_VERSION_MINOR)
static char* __env[] = { "ARCH=ARM32", "MCU=STM32F746ZI", "PLATFORM=ARM_FW_PLAYGROUND", "OS=FreeRTOS", "USERNAME=STM32",
                         NEWLIB_ENV, OS_VERSION_ENV, GNUC_ENV, DEBUG_ENV, "BUILDTIME=" xstr(__TIME__), "BUILDDATE=" xstr(__DATE__),
						 NULL
                       };
char** environ = &__env[0];
// ----------------------------------------------------------------------------

/*!
 * \brief is used to provide an overwritable stdout channel which can be used
 * for debugging purposes in the application
 * \param chr
 */
// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) void __stdout_put_char( int chr );
// ----------------------------------------------------------------------------

/*!
 * \brief is used to provide an overwritable stdin channel which can be used
 * for debugging purposes in the application
 */
// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) int  __stdin_get_char( void )
{
    return -1;
}
// ----------------------------------------------------------------------------

/*!
 * \brief is used to provide an overwritable clock tick function which is used
 * by the stdlib
 */
// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) unsigned long long __stdlib_get_system_clock_ticks( void )
{
#if defined(INC_FREERTOS_H)
	return (unsigned long long)xTaskGetTickCount();
#else
    return 0;
#endif
}
// ----------------------------------------------------------------------------

/*!
 * \brief semihosting initialization function which should be called when _start
 * is called. afterwards main would be called.
 */
// ----------------------------------------------------------------------------
void initialise_monitor_handles( void )
// ----------------------------------------------------------------------------
{

}

/*!
 * \brief this function will be called after initialization of the .bss, .text,
 * and .data segment of the application and while the stdlib begins to initialize
 * its requirements. This is done by calling the __libc_init_array in _start.S routine
 * before calling the main function. By tagging a function with the "constructor"
 * attribute, the compiler will add the function pointer to this initialization list.
 * \attention This is a GCC attribute. In case the application is compiled with another
 * compiler, this attribute must be ported as well!
 */
// ----------------------------------------------------------------------------
__attribute__((constructor))
void initialise_stdlib_abstraction( void )
// ----------------------------------------------------------------------------
{
    initialise_monitor_handles();

#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_STDOUT)
    stdioSemaphore = xSemaphoreCreateRecursiveMutex();

    if ( stdioSemaphore == 0 )
    {
        vAssertCalled( __FILE__, __LINE__ );
    }
#endif
#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_MALLOC)
    mallocSemaphore = xSemaphoreCreateRecursiveMutex();

    if ( mallocSemaphore == 0 )
    {
        vAssertCalled( __FILE__, __LINE__ );
    }
#endif
#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_ENV)
    envSemaphore = xSemaphoreCreateRecursiveMutex();

    if ( envSemaphore == 0 )
    {
        vAssertCalled( __FILE__, __LINE__ );
    }
#endif
}

/*!
 * \brief returns the process ID (PID) of the calling process.
 * \attention we have no real OS and so we only return PID 1 because
 * this is the only running process. Everything is in "kernel space" or
 * "machine mode"
 */
// ----------------------------------------------------------------------------
int _getpid( void )
// ----------------------------------------------------------------------------
{
    return 1;
}

/*!
 * \brief  The command kill sends the specified signal to the specified
 * process.
 * \attention we have no real OS and so we cannot kill our only running
 * process with PID 1
 */
// ----------------------------------------------------------------------------
int _kill( int pid, int sig )
// ----------------------------------------------------------------------------
{
    ( void )pid;
    ( void )sig;

    if ( _impure_ptr != 0 )
    {
        _impure_ptr->_errno = EINVAL;
    }
    errno = EINVAL;
    return -1;
}

/*!
 * \brief The command exit is used to stop the process or application
 * \attention if the application has called exit, we do must not return
 * from this function anymore
 */
// ----------------------------------------------------------------------------
__attribute__( ( noreturn ) )
void _exit ( int status )
// ----------------------------------------------------------------------------
{
    _kill( status, -1 );
    while ( 1 ) {}
}

/*!
 * \brief attempts to read up to 'len' bytes from file descriptor 'file'
 * into the buffer starting at 'ptr'.
 * \note currently there is only support for stdin. If any other devices
 * have to be integrated as well, one should implement an abstraction layer
 * which delegates the calls directly to the required device or ressource.
 * An example can be seen in altera board support packages with the "dev" struct
 */
// ----------------------------------------------------------------------------
int _read( int file, char* ptr, int len )
// ----------------------------------------------------------------------------
{
    ( void )file;

    int DataIdx;
    int resLen = 0;

    if ( file == STDIN_FILENO )
    {
        for ( DataIdx = 0; DataIdx < len; )
        {
            int result = __stdin_get_char();
            if ( result == EOF )
            {
                if ( resLen == 0 )
                {
                    resLen = EOF;
                }
                break;
            }

            *ptr++ = ( char )result;
            resLen++;
            DataIdx++;

        }
    }
    else
    {
        if ( _impure_ptr != 0 )
        {
            _impure_ptr->_errno = EBADF;
        }
        errno = EBADF;
        return -1;
    }

    return resLen;
}

/*!
 * \brief attempts to write 'len' bytes from file descriptor 'file'
 * out of the buffer starting at 'ptr'.
 * \note currently there is only support for stdout and stderr. If any other devices
 * have to be integrated as well, one should implement an abstraction layer
 * which delegates the calls directly to the required device or ressource.
 * An example can be seen in altera board support packages with the "dev" struct
 */
// ----------------------------------------------------------------------------
int _write( int file, char* ptr, int len )
// ----------------------------------------------------------------------------
{
    ( void )file;

    int DataIdx, locked = 0;

    if ( file == STDOUT_FILENO || file == STDERR_FILENO )
    {

#if  defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_STDOUT)
        if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
        {
            xSemaphoreTakeRecursive( stdioSemaphore, -1 );
            locked = 1;
        }
#endif

        if (file == STDERR_FILENO)
        {
        	__stdout_put_char('\033');
        	__stdout_put_char('[');
        	__stdout_put_char('3');
        	__stdout_put_char('1');
        	__stdout_put_char('m');
        }
        for ( DataIdx = 0; DataIdx < len; DataIdx++ )
        {
            __stdout_put_char( ptr[DataIdx] );
        }
        if (file == STDERR_FILENO)
        {
        	__stdout_put_char('\033');
        	__stdout_put_char('[');
        	__stdout_put_char('0');
        	__stdout_put_char('m');
        }

#if defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_STDOUT)
        if ( locked )
        {
            xSemaphoreGiveRecursive( stdioSemaphore );
        }
#endif

        return len;
    }
    else
    {
        if ( _impure_ptr != 0 )
        {
            _impure_ptr->_errno = EBADF;
        }
        errno = EBADF;
        return -1;
    }
}

/*!
 * \brief attempts to close a descriptor.
 * \note currently there is no support for this function. If any other devices
 * have to be integrated as well, one should implement an abstraction layer
 * which delegates the calls directly to the required device or ressource.
 * An example can be seen in altera board support packages with the "dev" struct
 */
// ----------------------------------------------------------------------------
int _close( int file )
// ----------------------------------------------------------------------------
{
    ( void )file;

    return -1;
}

/*!
 * \brief These function returns information about a file, in the buffer
 * pointed to by stat
 * \note currently there is only support for stdin/out/err function. If any other devices
 * have to be integrated as well, one should implement an abstraction layer
 * which delegates the calls directly to the required device or ressource.
 * An example can be seen in altera board support packages with the "dev" struct
 */
// ----------------------------------------------------------------------------
int _fstat( int file, struct stat* st )
// ----------------------------------------------------------------------------
{
    st->st_dev    = file;
    st->st_size   = 0;
    st->st_blocks = 0;

    if ( file >= 1 && file <= 3 )
    {
        st->st_mode = S_IFCHR;
        return 0;
    }
    else
    {
        st->st_mode = S_IFREG;
        return -1;
    }
}


/*!
 * \brief checks wether a descriptor 'file' is a TTY device or not
 * \note currently there is only support for stdin/out/err function. If any other devices
 * have to be integrated as well, one should implement an abstraction layer
 * which delegates the calls directly to the required device or ressource.
 * An example can be seen in altera board support packages with the "dev" struct
 */
// ----------------------------------------------------------------------------
int _isatty( int file )
// ----------------------------------------------------------------------------
{
    // is stdin, stdout or stderror
    if ( file >= 0 && file <= 2 )
    {
        return file;
    }
    return 0;
}

// ----------------------------------------------------------------------------
int _lseek( int file, int ptr, int dir )
// ----------------------------------------------------------------------------
{
    ( void )file;
    ( void )ptr;
    ( void )dir;

    return 0;
}

// ----------------------------------------------------------------------------
int _open( char* path, int flags, ... )
// ----------------------------------------------------------------------------
{
    ( void )path;
    ( void )flags;

    /* Pretend like we always fail */
    return -1;
}

// ----------------------------------------------------------------------------
int _wait( int* status )
// ----------------------------------------------------------------------------
{
    ( void )status;

    if ( _impure_ptr != 0 )
    {
        _impure_ptr->_errno = ECHILD;
    }
    errno = ECHILD;
    return -1;
}

// ----------------------------------------------------------------------------
int _unlink( char* name )
// ----------------------------------------------------------------------------
{
    ( void )name;

    if ( _impure_ptr != 0 )
    {
        _impure_ptr->_errno = ENOENT;
    }
    errno = ENOENT;
    return -1;
}

// ----------------------------------------------------------------------------
int _times( struct tms* buf )
// ----------------------------------------------------------------------------
{
    buf->tms_utime = buf->tms_cutime = 0; // we have no user space!
    buf->tms_stime = buf->tms_cstime = __stdlib_get_system_clock_ticks( ); // everything runs in one process!
    return 0;
}

// ----------------------------------------------------------------------------
int _stat( char* file, struct stat* st )
// ----------------------------------------------------------------------------
{
    ( void )file;
    ( void )st;

    st->st_mode = S_IFCHR;
    return 0;
}

// ----------------------------------------------------------------------------
int _link( char* old, char* new )
// ----------------------------------------------------------------------------
{
    ( void )old;
    ( void )new;

    if ( _impure_ptr != 0 )
    {
        _impure_ptr->_errno = EMLINK;
    }
    errno = EMLINK;
    return -1;
}

// ----------------------------------------------------------------------------
int _fork( void )
// ----------------------------------------------------------------------------
{
    if ( _impure_ptr != 0 )
    {
        _impure_ptr->_errno = EAGAIN;
    }
    errno = EAGAIN;
    return -1;
}

// ----------------------------------------------------------------------------
int _execve( char* name, char** argv, char** env )
// ----------------------------------------------------------------------------
{
    ( void )env;
    ( void )name;
    ( void )argv;

    if ( _impure_ptr != 0 )
    {
        _impure_ptr->_errno = ENOMEM;
    }
    errno = ENOMEM;
    return -1;
}

// ================================================================================================
// External routines required by newlib's malloc (sbrk/_sbrk, __malloc_lock/unlock)
// ================================================================================================

#if !defined(INC_FREERTOS_H)

// ----------------------------------------------------------------------------
caddr_t _sbrk( int incr )
// ----------------------------------------------------------------------------
{
	register char* stack_ptr asm( "sp" );

    extern char end asm( "end" );
    static char* heap_end;
    char* prev_heap_end;

    if ( heap_end == 0 )
    {
        heap_end = &end;
    }

    prev_heap_end = heap_end;
    if ( heap_end + incr > stack_ptr )
    {
//      write(1, "Heap and stack collision\n", 25);
//      abort();
        if ( _impure_ptr != 0 )
        {
            _impure_ptr->_errno = ENOMEM;
        }
        errno = ENOMEM;
        return ( caddr_t ) -1;
    }

    heap_end += incr;

    return ( caddr_t ) prev_heap_end;
}

#else

static int heapBytesRemaining;
static unsigned int stickyHeapBytesRemaining;
uint32_t TotalHeapSize;

// ----------------------------------------------------------------------------
static UBaseType_t malLock_uxSavedInterruptStatus;

// ----------------------------------------------------------------------------
void* _sbrk_r( struct _reent* pReent, int incr )
// ----------------------------------------------------------------------------
{
	( void )malLock_uxSavedInterruptStatus;
    ( void )pReent;
    ( void )incr;
    register char* stack_ptr asm( "sp" );

    // make sure to calculate the correct heap size and bytes remaining at the first call!
    if( TotalHeapSize == 0 )
    {
        TotalHeapSize = heapBytesRemaining = stickyHeapBytesRemaining
                                             = ( int )( ( &__HeapLimit ) - ( &__HeapBase ) )
#ifdef configISR_STACK_SIZE_WORDS
                                               - ( configISR_STACK_SIZE_WORDS * 4 )
#endif
                                               ;
    };
    static char* currentHeapEnd = &__HeapBase;
    char* limit = ( xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED ) ?
                  stack_ptr   :  // Before scheduler is started, limit is stack pointer (risky!)
                  &__HeapLimit
#ifdef configISR_STACK_SIZE_WORDS
                  - ( configISR_STACK_SIZE_WORDS * 4 )
#endif
                  ; // Once running, OK to reuse all remaining RAM except ISR stack (MSP) stack
    DRN_ENTER_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
    if ( currentHeapEnd + incr > limit )
    {
        // Ooops, no more memory available...
#if( configUSE_MALLOC_FAILED_HOOK == 1 )
        {
            extern void vApplicationMallocFailedHook( void );
            DRN_EXIT_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
            vApplicationMallocFailedHook();
        }
#elif defined(configHARD_STOP_ON_MALLOC_FAILURE)
        // If you want to alert debugger or halt...
        // WARNING: brkpt instruction may prevent watchdog operation...
        while( 1 )
        {
            __asm( "bkpt #0" );
        }; // Stop in GUI as if at a breakpoint (if debugging, otherwise loop forever)
#else
        // Default, if you prefer to believe your application will gracefully trap out-of-memory...
        pReent->_errno = ENOMEM; // newlib's thread-specific errno
        DRN_EXIT_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
#endif
        return ( char* ) -1; // the malloc-family routine that called sbrk will return 0
    }

    char* previousHeapEnd = currentHeapEnd;
    currentHeapEnd += incr;
    heapBytesRemaining -= incr;

    // implement the statistical feature to provide the minimum free heap size feature
    // of FreeRTOS statistics
    if ( stickyHeapBytesRemaining > xPortGetFreeHeapSize() )
    {
        stickyHeapBytesRemaining = xPortGetFreeHeapSize();
    }

    DRN_EXIT_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
    return ( char* ) previousHeapEnd;
}

/*! \brief non-reentrant sbrk uses is actually reentrant by using current context
 *  because the current _reent structure is pointed to by global _impure_ptr
 */
// ----------------------------------------------------------------------------
void* sbrk( ptrdiff_t __incr )
// ----------------------------------------------------------------------------
{
    return _sbrk_r( _impure_ptr, __incr );
}

//! _sbrk is a synonym for sbrk.
// ----------------------------------------------------------------------------
char* _sbrk( int incr )
// ----------------------------------------------------------------------------
{
    return sbrk( incr );
}

// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) void __malloc_lock( struct _reent* r )
// ----------------------------------------------------------------------------
{
    ( void )r;

#if  defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_MALLOC)
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING  )
    {
        xSemaphoreTakeRecursive( mallocSemaphore, -1 );
    }
#endif

    DRN_ENTER_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
}

// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) void __malloc_unlock( struct _reent* r )
// ----------------------------------------------------------------------------
{
    ( void )r;

#if  defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_MALLOC)
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
    {
        xSemaphoreGiveRecursive( mallocSemaphore );
    }
#endif

    DRN_EXIT_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
}

// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) void __env_lock( void )
// ----------------------------------------------------------------------------
{
#if  defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_ENV)
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
    {
        xSemaphoreTakeRecursive( envSemaphore, -1 );
    }
#endif

    DRN_ENTER_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
}

// ----------------------------------------------------------------------------
__attribute__( ( weak ) ) void __env_unlock( void )
// ----------------------------------------------------------------------------
{
#if  defined(INC_FREERTOS_H) && defined(MV_SYSCALL_USE_EXCLUSIVE_LOCK_FOR_ENV)
    if ( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
    {
        xSemaphoreGiveRecursive( envSemaphore );
    }
#endif

    DRN_EXIT_CRITICAL_SECTION( malLock_uxSavedInterruptStatus );
}

// ----------------------------------------------------------------------------
void* pvPortMalloc( size_t xSize )
// ----------------------------------------------------------------------------
{
    void* p = malloc( xSize );
    return p;
}

// ----------------------------------------------------------------------------
void vPortFree( void* pv )
// ----------------------------------------------------------------------------
{
    free( pv );
}

// ----------------------------------------------------------------------------
size_t xPortGetFreeHeapSize( void )
// ----------------------------------------------------------------------------
{
	static int lockGuard = 0;
	if ( lockGuard != 0 ) return stickyHeapBytesRemaining;
	lockGuard = 1;
    struct mallinfo mi = mallinfo();
    size_t result = mi.fordblks + heapBytesRemaining;
    lockGuard = 0;
    return result;

}

// ----------------------------------------------------------------------------
size_t xPortGetMinimumEverFreeHeapSize ( void )
// ----------------------------------------------------------------------------
{
    return stickyHeapBytesRemaining;
}

//! No implementation needed, but stub provided in case application already calls vPortInitialiseBlocks
// ----------------------------------------------------------------------------
void vPortInitialiseBlocks( void )
// ----------------------------------------------------------------------------
{
    return;
}


#endif
