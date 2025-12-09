// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/parse.h
 * \brief Client message parsing interface
 * \author William Ferrell
 * \author Selene Scriven
 * \date 1999
 *
 * \features
 * - Client message parsing and processing
 * - Protocol command interpretation
 * - Message queue management
 * - Client communication handling
 *
 * \usage
 * - Called from main server loop to process pending client messages
 * - Handles all types of client commands and requests
 * - Manages client protocol state and validation
 *
 * \details Provides interface for parsing and processing client messages
 * received by the LCDd server. Handles communication protocol parsing
 * and command dispatching.
 */

#ifndef PARSE_H
#define PARSE_H

/**
 * \brief Parses and processes all pending client messages
 *
 * \details Processes all queued messages from connected clients,
 * parsing protocol commands and dispatching them to appropriate
 * handlers. This function is typically called from the main
 * server event loop.
 */
void parse_all_client_messages(void);

#endif
