/*
 * ConsoleConfig.h
 *
 *  Created on: Dec 9, 2024
 *      Author: Thorsten
 */

 /*! \file */

/*!
 * ATTENTION, this header is only a template, the user must provide its own implementation in the user project
 */

#ifndef INC_CONSOLE_CONSOLECONFIG_H_
#define INC_CONSOLE_CONSOLECONFIG_H_

/*!
 * The username in the console which is printed at least on power up before changing the username
 */
#define CONSOLE_USERNAME  "STM32"

/*!
 * The username ^can be changed on runtime which is enabled with this DEFINE
 */
#define CONSOLE_USE_DYNAMIC_USERNAME 1

/*!
 * specifies the maximum number of entries of the "last commands" buffer
 */
#define CONSOLE_LINE_HISTORY 8

/*!
 * Specifies the number of chars per line. When exceeding this number without a newline,
 * the console prints a buffer overflow and clears the input. All inputs are then invalidated within
 * this line
 */
#define CONSOLE_LINE_SIZE 120

/*!
 * Specifies the maximum number of chars per command
 */
#define CONSOLE_COMMAND_MAX_LENGTH 64

/*!
 * Specifies the maximum number of chars per command help text
 */
#define CONSOLE_HELP_MAX_LENGTH 512


#endif /* INC_CONSOLE_CONSOLECONFIG_H_ */
