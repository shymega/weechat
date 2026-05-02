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

/* IRCv3 draft/chathistory support */

#include <stdlib.h>
#include <string.h>

#include "../weechat-plugin.h"
#include "irc.h"
#include "irc-chathistory.h"
#include "irc-server.h"


/*
 * Returns 1 if the draft/chathistory capability is enabled on the server,
 * 0 otherwise.
 */

int
irc_chathistory_enabled (struct t_irc_server *server)
{
    if (!server)
        return 0;

    return weechat_hashtable_has_key (server->cap_list,
                                      IRC_CHATHISTORY_CAP);
}

/*
 * Sends a CHATHISTORY command to the server.
 *
 * For subcommands with two anchors (BETWEEN), pass both anchor1 and anchor2.
 * For subcommands with one anchor (BEFORE, AFTER, AROUND, LATEST), pass the
 * anchor in anchor1 and NULL in anchor2.
 * For TARGETS, target is NULL and both anchors are timestamps.
 *
 * The anchor format per the IRCv3 spec is one of:
 *   timestamp=YYYY-MM-DDThh:mm:ss.sssZ
 *   msgid=<id>
 *   * (wildcard, used with LATEST to get most recent messages)
 */

void
irc_chathistory_send (struct t_irc_server *server,
                      const char *subcommand,
                      const char *target,
                      const char *anchor1,
                      const char *anchor2,
                      int limit)
{
    if (!server || !subcommand || !anchor1)
        return;

    if (!irc_chathistory_enabled (server))
        return;

    if (anchor2)
    {
        /* two-anchor form: BETWEEN or TARGETS */
        if (target)
        {
            irc_server_sendf (server, IRC_SERVER_SEND_OUTQ_PRIO_HIGH, NULL,
                              "CHATHISTORY %s %s %s %s %d",
                              subcommand, target, anchor1, anchor2, limit);
        }
        else
        {
            /* TARGETS: no channel target */
            irc_server_sendf (server, IRC_SERVER_SEND_OUTQ_PRIO_HIGH, NULL,
                              "CHATHISTORY %s %s %s %d",
                              subcommand, anchor1, anchor2, limit);
        }
    }
    else
    {
        /* one-anchor form: LATEST, BEFORE, AFTER, AROUND */
        irc_server_sendf (server, IRC_SERVER_SEND_OUTQ_PRIO_HIGH, NULL,
                          "CHATHISTORY %s %s %s %d",
                          subcommand, target, anchor1, limit);
    }
}
