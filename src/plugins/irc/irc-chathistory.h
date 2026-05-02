/*
 * SPDX-FileCopyrightText: 2024-2026 WeeChat contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of WeeChat, the extensible chat client.
 *
 * WeeChat is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WeeChat.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WEECHAT_PLUGIN_IRC_CHATHISTORY_H
#define WEECHAT_PLUGIN_IRC_CHATHISTORY_H

/* IRCv3 draft/chathistory capability name */
#define IRC_CHATHISTORY_CAP "draft/chathistory"

/* CHATHISTORY subcommands */
#define IRC_CHATHISTORY_SUB_LATEST   "LATEST"
#define IRC_CHATHISTORY_SUB_BEFORE   "BEFORE"
#define IRC_CHATHISTORY_SUB_AFTER    "AFTER"
#define IRC_CHATHISTORY_SUB_AROUND   "AROUND"
#define IRC_CHATHISTORY_SUB_BETWEEN  "BETWEEN"
#define IRC_CHATHISTORY_SUB_TARGETS  "TARGETS"

/* Default number of messages to request */
#define IRC_CHATHISTORY_DEFAULT_LIMIT 100

struct t_irc_server;

extern int irc_chathistory_enabled (struct t_irc_server *server);
extern void irc_chathistory_send (struct t_irc_server *server,
                                  const char *subcommand,
                                  const char *target,
                                  const char *anchor1,
                                  const char *anchor2,
                                  int limit);
extern void irc_chathistory_auto_fetch_on_join (struct t_irc_server *server,
                                                const char *channel);

#endif /* WEECHAT_PLUGIN_IRC_CHATHISTORY_H */
