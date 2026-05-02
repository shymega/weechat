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

/* Test IRC chathistory functions */

#include "CppUTest/TestHarness.h"

#include "tests.h"
#include "tests-record.h"

extern "C"
{
#include <string.h>
#include "src/core/core-arraylist.h"
#include "src/core/core-config-file.h"
#include "src/core/core-hashtable.h"
#include "src/core/core-hook.h"
#include "src/core/core-input.h"
#include "src/core/core-string.h"
#include "src/gui/gui-buffer.h"
#include "src/gui/gui-line.h"
#include "src/plugins/plugin.h"
#include "src/plugins/irc/irc-batch.h"
#include "src/plugins/irc/irc-chathistory.h"
#include "src/plugins/irc/irc-channel.h"
#include "src/plugins/irc/irc-config.h"
#include "src/plugins/irc/irc-protocol.h"
#include "src/plugins/irc/irc-server.h"
#include "src/plugins/logger/logger-config.h"
#include "src/plugins/typing/typing-config.h"
#include "src/plugins/typing/typing-status.h"
#include "src/plugins/xfer/xfer-buffer.h"
}

#define IRC_FAKE_SERVER "fake"

#define RECV(__irc_msg)                                                 \
    server_recv (__irc_msg);

#define CHECK_CHAN(__prefix, __message, __tags)                         \
    if (!record_search ("irc." IRC_FAKE_SERVER ".#test", __prefix,      \
                        __message, __tags))                             \
    {                                                                   \
        char **msg = build_error (                                      \
            "Channel message not displayed",                            \
            __prefix,                                                   \
            __message,                                                  \
            __tags,                                                     \
            "All messages displayed");                                  \
        record_dump (msg);                                              \
        FAIL(string_dyn_free (msg, 0));                                 \
    }

#define CHECK_NO_MSG                                                    \
    if (arraylist_size (recorded_messages) > 0)                         \
    {                                                                   \
        char **msg = build_error (                                      \
            "Unexpected message(s) displayed",                          \
            NULL,                                                       \
            NULL,                                                       \
            NULL,                                                       \
            NULL);                                                      \
        record_dump (msg);                                              \
        FAIL(string_dyn_free (msg, 0));                                 \
    }

#define CHECK_SENT(__message)                                           \
    if ((__message != NULL)                                             \
        && !arraylist_search (sent_messages, (void *)__message,         \
                              NULL, NULL))                              \
    {                                                                   \
        char **msg = build_error (                                      \
            "Message not sent to the IRC server",                       \
            NULL,                                                       \
            __message,                                                  \
            NULL,                                                       \
            "All messages sent");                                       \
        sent_msg_dump (msg);                                            \
        FAIL(string_dyn_free (msg, 0));                                 \
    }                                                                   \
    else if ((__message == NULL)                                        \
             && (arraylist_size (sent_messages) > 0))                   \
    {                                                                   \
        char **msg = build_error (                                      \
            "Unexpected response(s) sent to the IRC server",            \
            NULL,                                                       \
            NULL,                                                       \
            NULL,                                                       \
            NULL);                                                      \
        sent_msg_dump (msg);                                            \
        FAIL(string_dyn_free (msg, 0));                                 \
    }

#define SRV_INIT                                                        \
    RECV(":server 001 alice :Welcome on this server, alice!");          \
    CHECK_CHAN_OR_SRV_INIT;

#define CHECK_CHAN_OR_SRV_INIT                                          \
    record_search ("irc.server." IRC_FAKE_SERVER, "--",                 \
                   "Welcome on this server, alice!",                    \
                   "irc_001,irc_numeric,nick_server,log3");

#define SRV_INIT_JOIN                                                   \
    SRV_INIT;                                                           \
    RECV(":alice!user_a@host_a JOIN #test");                            \
    CHECK_CHAN("-->", "alice (user_a@host_a) has joined #test",         \
               "irc_join,nick_alice,host_user_a@host_a,log4");


TEST_GROUP(IrcChathistory)
{
};

TEST_GROUP(IrcChathistoryWithServer)
{
    struct t_irc_server *ptr_server = NULL;
    struct t_arraylist *sent_messages = NULL;
    struct t_hook *hook_signal_irc_out = NULL;

    void server_recv (const char *command)
    {
        char str_command[4096];

        record_start ();
        arraylist_clear (sent_messages);

        snprintf (str_command, sizeof (str_command),
                  "/command -buffer irc.server." IRC_FAKE_SERVER " irc "
                  "/server fakerecv \"%s\"",
                  command);
        run_cmd_quiet (str_command);

        record_stop ();
    }

    char **build_error (const char *msg1,
                        const char *prefix,
                        const char *message,
                        const char *tags,
                        const char *msg2)
    {
        char **msg;

        msg = string_dyn_alloc (1024);
        string_dyn_concat (msg, msg1, -1);
        if (message)
        {
            string_dyn_concat (msg, ": prefix=\"", -1);
            string_dyn_concat (msg, prefix, -1);
            string_dyn_concat (msg, "\", message=\"", -1);
            string_dyn_concat (msg, message, -1);
            string_dyn_concat (msg, "\", tags=\"", -1);
            string_dyn_concat (msg, tags, -1);
            string_dyn_concat (msg, "\"\n", -1);
        }
        else
        {
            string_dyn_concat (msg, ":\n", -1);
        }
        if (msg2)
        {
            string_dyn_concat (msg, msg2, -1);
            string_dyn_concat (msg, ":\n", -1);
        }
        return msg;
    }

    static int signal_irc_out_cb (const void *pointer, void *data,
                                  const char *signal, const char *type_data,
                                  void *signal_data)
    {
        (void) data;
        (void) signal;
        (void) type_data;

        if (signal_data)
        {
            arraylist_add ((struct t_arraylist *)pointer,
                           strdup ((const char *)signal_data));
        }

        return WEECHAT_RC_OK;
    }

    static int sent_msg_cmp_cb (void *data, struct t_arraylist *arraylist,
                                void *pointer1, void *pointer2)
    {
        (void) data;
        (void) arraylist;

        return strcmp ((char *)pointer1, (char *)pointer2);
    }

    static void sent_msg_free_cb (void *data, struct t_arraylist *arraylist,
                                  void *pointer)
    {
        (void) data;
        (void) arraylist;

        free (pointer);
    }

    void sent_msg_dump (char **msg)
    {
        int i;

        for (i = 0; i < arraylist_size (sent_messages); i++)
        {
            string_dyn_concat (msg, "  \"", -1);
            string_dyn_concat (msg,
                               (const char *)arraylist_get (sent_messages, i),
                               -1);
            string_dyn_concat (msg, "\"\n", -1);
        }
    }

    void setup ()
    {
        if (sent_messages)
        {
            arraylist_clear (sent_messages);
        }
        else
        {
            sent_messages = arraylist_new (16, 0, 1,
                                           &sent_msg_cmp_cb, NULL,
                                           &sent_msg_free_cb, NULL);
        }

        if (!hook_signal_irc_out)
        {
            hook_signal_irc_out = hook_signal (
                NULL,
                IRC_FAKE_SERVER ",irc_out1_*",
                &signal_irc_out_cb, sent_messages, NULL);
        }

        config_file_option_set (logger_config_look_backlog, "0", 1);

        run_cmd_quiet ("/mute /server add " IRC_FAKE_SERVER " fake:127.0.0.1 "
                       "-nicks=alice,nick2,nick3");
        run_cmd_quiet ("/connect " IRC_FAKE_SERVER);
        ptr_server = irc_server_search (IRC_FAKE_SERVER);
    }

    void teardown ()
    {
        run_cmd_quiet ("/mute /disconnect " IRC_FAKE_SERVER);
        run_cmd_quiet ("/mute /server del " IRC_FAKE_SERVER);
        ptr_server = NULL;

        config_file_option_reset (logger_config_look_backlog, 1);
    }
};

/*
 * Test functions:
 *   irc_chathistory_enabled
 */

TEST(IrcChathistory, Enabled)
{
    struct t_irc_server *server;

    /* NULL server: return 0 */
    LONGS_EQUAL(0, irc_chathistory_enabled (NULL));

    server = irc_server_alloc ("test_ch_enabled");
    CHECK(server);

    /* cap not in cap_list: return 0 */
    LONGS_EQUAL(0, irc_chathistory_enabled (server));

    /* add cap to cap_list: return 1 */
    hashtable_set (server->cap_list, IRC_CHATHISTORY_CAP, NULL);
    LONGS_EQUAL(1, irc_chathistory_enabled (server));

    /* remove cap: return 0 again */
    hashtable_remove (server->cap_list, IRC_CHATHISTORY_CAP);
    LONGS_EQUAL(0, irc_chathistory_enabled (server));

    irc_server_free (server);
}

/*
 * Test functions:
 *   irc_chathistory_send
 */

TEST(IrcChathistoryWithServer, Send)
{
    SRV_INIT;

    /* NULL server: no crash */
    irc_chathistory_send (NULL, "LATEST", "#test", "*", NULL, 50);

    /* NULL subcommand: no crash */
    irc_chathistory_send (ptr_server, NULL, "#test", "*", NULL, 50);

    /* NULL anchor1: no crash */
    irc_chathistory_send (ptr_server, "LATEST", "#test", NULL, NULL, 50);

    /* cap not enabled: no message sent */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_LATEST,
                          "#test", "*", NULL, 50);
    record_stop ();
    CHECK_SENT(NULL);

    /* enable cap */
    hashtable_set (ptr_server->cap_list, IRC_CHATHISTORY_CAP, NULL);

    /* LATEST */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_LATEST,
                          "#test", "*", NULL, 50);
    record_stop ();
    CHECK_SENT("CHATHISTORY LATEST #test * 50");

    /* BEFORE with timestamp anchor */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_BEFORE,
                          "#test", "timestamp=2024-01-01T00:00:00.000Z",
                          NULL, 100);
    record_stop ();
    CHECK_SENT("CHATHISTORY BEFORE #test "
               "timestamp=2024-01-01T00:00:00.000Z 100");

    /* AFTER with msgid anchor */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_AFTER,
                          "#test", "msgid=abc123", NULL, 100);
    record_stop ();
    CHECK_SENT("CHATHISTORY AFTER #test msgid=abc123 100");

    /* AROUND */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_AROUND,
                          "#test", "msgid=xyz789", NULL, 50);
    record_stop ();
    CHECK_SENT("CHATHISTORY AROUND #test msgid=xyz789 50");

    /* BETWEEN */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_BETWEEN,
                          "#test",
                          "timestamp=2024-01-01T00:00:00.000Z",
                          "timestamp=2024-01-01T12:00:00.000Z",
                          200);
    record_stop ();
    CHECK_SENT("CHATHISTORY BETWEEN #test "
               "timestamp=2024-01-01T00:00:00.000Z "
               "timestamp=2024-01-01T12:00:00.000Z 200");

    /* TARGETS (no channel target) */
    record_start ();
    arraylist_clear (sent_messages);
    irc_chathistory_send (ptr_server, IRC_CHATHISTORY_SUB_TARGETS,
                          NULL,
                          "timestamp=2024-01-01T00:00:00.000Z",
                          "timestamp=2024-01-01T12:00:00.000Z",
                          50);
    record_stop ();
    CHECK_SENT("CHATHISTORY TARGETS "
               "timestamp=2024-01-01T00:00:00.000Z "
               "timestamp=2024-01-01T12:00:00.000Z 50");

    hashtable_remove (ptr_server->cap_list, IRC_CHATHISTORY_CAP);
}

/*
 * Test functions:
 *   irc_protocol_cb_batch (chathistory type)
 *   irc_batch_process_messages (chathistory type)
 */

TEST(IrcChathistoryWithServer, BatchReceive)
{
    SRV_INIT_JOIN;

    /* enable required capabilities */
    hashtable_set (ptr_server->cap_list, "batch", NULL);
    hashtable_set (ptr_server->cap_list, "server-time", NULL);
    hashtable_set (ptr_server->cap_list, IRC_CHATHISTORY_CAP, NULL);

    /*
     * receive a chathistory batch with two PRIVMSG messages:
     * the batch start and individual messages should produce no output until
     * the batch ends
     */
    RECV(":server BATCH +histref chathistory #test");
    CHECK_NO_MSG;

    RECV("@batch=histref;time=2024-01-01T10:00:00.000Z "
         ":bob!user_b@host_b PRIVMSG #test :first historical message");
    CHECK_NO_MSG;

    RECV("@batch=histref;time=2024-01-01T11:00:00.000Z "
         ":alice!user_a@host_a PRIVMSG #test :second historical message");
    CHECK_NO_MSG;

    /* batch end: messages are now processed */
    RECV(":server BATCH -histref");
    CHECK_CHAN("bob", "first historical message",
               "irc_privmsg,irc_tag_batch=histref,"
               "irc_tag_time=2024-01-01T10:00:00.000Z,"
               "irc_batch_type_chathistory,notify_message,prefix_nick_248,"
               "nick_bob,host_user_b@host_b,log1");
    CHECK_CHAN("alice", "second historical message",
               "irc_privmsg,irc_tag_batch=histref,"
               "irc_tag_time=2024-01-01T11:00:00.000Z,"
               "irc_batch_type_chathistory,notify_message,prefix_nick_248,"
               "nick_alice,host_user_a@host_a,log1");

    /* batch must be cleared after processing */
    POINTERS_EQUAL(NULL, irc_batch_search (ptr_server, "histref"));

    /* server chathistory context must be cleared after processing */
    POINTERS_EQUAL(NULL, ptr_server->chathistory_buffer);
    LONGS_EQUAL(-1, ptr_server->chathistory_before_line_id);

    hashtable_remove (ptr_server->cap_list, "batch");
    hashtable_remove (ptr_server->cap_list, "server-time");
    hashtable_remove (ptr_server->cap_list, IRC_CHATHISTORY_CAP);
}

/*
 * Test functions:
 *   irc_protocol_cb_batch (chathistory type)
 *   irc_batch_process_messages (chathistory type)
 *
 * Test that chathistory messages are inserted BEFORE existing lines in the
 * buffer, preserving chronological ordering.
 */

TEST(IrcChathistoryWithServer, BatchInsertBefore)
{
    struct t_irc_channel *ptr_channel;
    struct t_gui_line *ptr_first_line, *ptr_second_line, *ptr_third_line;

    SRV_INIT_JOIN;

    /* enable required capabilities */
    hashtable_set (ptr_server->cap_list, "batch", NULL);
    hashtable_set (ptr_server->cap_list, "server-time", NULL);
    hashtable_set (ptr_server->cap_list, IRC_CHATHISTORY_CAP, NULL);

    /*
     * print a "current session" message to the channel buffer
     * (this will be the first real line in the buffer after the join message)
     */
    RECV("@time=2024-01-01T13:00:00.000Z "
         ":charlie!user_c@host_c PRIVMSG #test :current session message");
    CHECK_CHAN("charlie", "current session message",
               "irc_privmsg,irc_tag_time=2024-01-01T13:00:00.000Z,"
               "notify_message,prefix_nick_248,nick_charlie,"
               "host_user_c@host_c,log1");

    /* get the channel and verify buffer has lines */
    ptr_channel = irc_channel_search (ptr_server, "#test");
    CHECK(ptr_channel);
    CHECK(ptr_channel->buffer);
    CHECK(ptr_channel->buffer->own_lines);
    CHECK(ptr_channel->buffer->own_lines->first_line);

    /*
     * receive a chathistory batch with historical messages (older timestamps)
     * these should be inserted BEFORE the existing lines
     */
    RECV(":server BATCH +histref chathistory #test");
    CHECK_NO_MSG;

    RECV("@batch=histref;time=2024-01-01T10:00:00.000Z "
         ":bob!user_b@host_b PRIVMSG #test :oldest history");
    CHECK_NO_MSG;

    RECV("@batch=histref;time=2024-01-01T11:00:00.000Z "
         ":alice!user_a@host_a PRIVMSG #test :newer history");
    CHECK_NO_MSG;

    /* batch end: messages processed and inserted before existing lines */
    RECV(":server BATCH -histref");
    CHECK_CHAN("bob", "oldest history",
               "irc_privmsg,irc_tag_batch=histref,"
               "irc_tag_time=2024-01-01T10:00:00.000Z,"
               "irc_batch_type_chathistory,notify_message,prefix_nick_248,"
               "nick_bob,host_user_b@host_b,log1");
    CHECK_CHAN("alice", "newer history",
               "irc_privmsg,irc_tag_batch=histref,"
               "irc_tag_time=2024-01-01T11:00:00.000Z,"
               "irc_batch_type_chathistory,notify_message,prefix_nick_248,"
               "nick_alice,host_user_a@host_a,log1");

    /*
     * Verify line insertion order in the buffer:
     * The buffer lines should now be, in order from first to last:
     *   [join message, oldest history, newer history, current session message]
     *
     * The "current session message" line (charlie's) should be AFTER the
     * two historical messages.
     */
    ptr_first_line = ptr_channel->buffer->own_lines->first_line;
    CHECK(ptr_first_line);

    /* traverse to find the chathistory messages; they come after the join */
    ptr_second_line = NULL;
    ptr_third_line = NULL;
    {
        struct t_gui_line *ptr_line = ptr_first_line;
        /* skip to lines with messages */
        while (ptr_line)
        {
            if (ptr_line->data->message
                && (strcmp (ptr_line->data->message, "oldest history") == 0))
            {
                ptr_second_line = ptr_line;
                ptr_third_line = ptr_line->next_line;
                break;
            }
            ptr_line = ptr_line->next_line;
        }
    }

    /* verify "oldest history" is found and has correct content */
    CHECK(ptr_second_line);
    STRCMP_EQUAL("oldest history", ptr_second_line->data->message);
    STRCMP_EQUAL("bob", ptr_second_line->data->prefix);

    /* verify "newer history" is immediately after "oldest history" */
    CHECK(ptr_third_line);
    STRCMP_EQUAL("newer history", ptr_third_line->data->message);
    STRCMP_EQUAL("alice", ptr_third_line->data->prefix);

    /* verify "current session message" is AFTER the two history messages */
    CHECK(ptr_third_line->next_line);
    STRCMP_EQUAL("current session message",
                 ptr_third_line->next_line->data->message);
    STRCMP_EQUAL("charlie", ptr_third_line->next_line->data->prefix);

    hashtable_remove (ptr_server->cap_list, "batch");
    hashtable_remove (ptr_server->cap_list, "server-time");
    hashtable_remove (ptr_server->cap_list, IRC_CHATHISTORY_CAP);
}

/*
 * Test functions:
 *   irc_protocol_cb_batch (chathistory type, empty buffer)
 *
 * Test that chathistory messages are appended normally when the buffer is
 * empty at the time of the request.
 */

TEST(IrcChathistoryWithServer, BatchEmptyBuffer)
{
    SRV_INIT;

    /* enable required capabilities */
    hashtable_set (ptr_server->cap_list, "batch", NULL);
    hashtable_set (ptr_server->cap_list, "server-time", NULL);
    hashtable_set (ptr_server->cap_list, IRC_CHATHISTORY_CAP, NULL);

    /*
     * Join a channel without any prior messages (fresh buffer).
     * The chathistory batch starts when the buffer has only the join line.
     * chathistory_before_line_id will be set to the join line's id so
     * history messages are inserted before it.
     */
    RECV(":alice!user_a@host_a JOIN #test");
    CHECK_CHAN("-->", "alice (user_a@host_a) has joined #test",
               "irc_join,nick_alice,host_user_a@host_a,log4");

    /* batch with no messages (empty history) */
    RECV(":server BATCH +emptyref chathistory #test");
    CHECK_NO_MSG;
    RECV(":server BATCH -emptyref");
    CHECK_NO_MSG;

    /* batch is cleared */
    POINTERS_EQUAL(NULL, irc_batch_search (ptr_server, "emptyref"));

    hashtable_remove (ptr_server->cap_list, "batch");
    hashtable_remove (ptr_server->cap_list, "server-time");
    hashtable_remove (ptr_server->cap_list, IRC_CHATHISTORY_CAP);
}

/*
 * Test functions:
 *   irc_protocol_cb_batch (chathistory type)
 *
 * Test that chathistory batch start populates the batch fields correctly.
 */

TEST(IrcChathistoryWithServer, BatchFields)
{
    struct t_irc_batch *ptr_batch;
    struct t_irc_channel *ptr_channel;

    SRV_INIT_JOIN;

    /* enable capabilities */
    hashtable_set (ptr_server->cap_list, "batch", NULL);
    hashtable_set (ptr_server->cap_list, IRC_CHATHISTORY_CAP, NULL);

    /* get the channel */
    ptr_channel = irc_channel_search (ptr_server, "#test");
    CHECK(ptr_channel);

    /* start a chathistory batch */
    RECV(":server BATCH +testref chathistory #test");
    CHECK_NO_MSG;

    ptr_batch = irc_batch_search (ptr_server, "testref");
    CHECK(ptr_batch);

    /* batch type must be "chathistory" */
    STRCMP_EQUAL("chathistory", ptr_batch->type);

    /* batch parameters must be the target channel */
    STRCMP_EQUAL("#test", ptr_batch->parameters);

    /* chathistory_buffer must point to the channel buffer */
    POINTERS_EQUAL(ptr_channel->buffer, ptr_batch->chathistory_buffer);

    /* chathistory_before_line_id must be >= 0 (buffer has lines from join) */
    CHECK(ptr_batch->chathistory_before_line_id >= 0);

    /* end the batch to clean up */
    RECV(":server BATCH -testref");
    CHECK_NO_MSG;

    hashtable_remove (ptr_server->cap_list, "batch");
    hashtable_remove (ptr_server->cap_list, IRC_CHATHISTORY_CAP);
}
