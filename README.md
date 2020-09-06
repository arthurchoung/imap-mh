# imap-mh

IMAP and the MH message system, together at last!

## Overview

Sync an IMAP mailbox with a local MH directory.

Written in C with no dynamic memory allocation, excluding what happens in the standard C library (most likely calls to fopen/printf).

nmh is recommended for MH functionality.

socat is required for communicating over the network.

## How to setup local directory

To compile:

$ sh build.sh

This results in the binary file 'imap-mh'.

Then:

$ cd

$ mkdir Mail

$ cd Mail

$ mkdir inbox

$ cd inbox

$ /path/to/imap-mh init

Answer the prompts to enter your username, password, and mailbox. For the mailbox, enter 'inbox' unless you want a specific mailbox. The information will be saved in the dotfiles '.username', '.password', and '.mailbox' in the current directory.

To download all the messages in the specified mailbox:

$ socat openssl:example.com:993 system:'/path/to/imap-mh download'

After downloading:

$ perl /path/to/make_mh_symlinks.pl

This generates a shell script to make a bunch of symlinks for MH. If it looks ok, run it:

$ perl /path/to/make_mh_symlinks.pl | sh

Your messages should be accessible now.

## How to update local directory

$ cd ~/Mail/inbox

$ socat openssl:example.com:993 system:'/path/to/imap-mh update'

This uses IMAP QRESYNC to perform the update. If changes have been made, the old symlinks will be removed, and will have to be re-generated:

$ perl /path/to/make_mh_symlinks.pl | sh

## How to wait for a change using IMAP IDLE

$ cd ~/Mail/inbox

$ socat openssl:example.com:993 system:'/path/to/imap-mh idle'

This uses IMAP IDLE, waits for an EXISTS message, then exits.

## Notes

This is a rather quick and dirty implementation.

Things to do:

* Flags
* Send local changes to server

## Legal

Copyright (c) 2020 Arthur Choung. All rights reserved.

Email: arthur -at- hotdoglinux.com

Released under the GNU General Public License, version 3.

For details on the license, refer to the LICENSE file.

