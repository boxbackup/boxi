#!/bin/sh

SUBJECT="BACKUP PROBLEM on host rocio"
SENDTO="chris"

if [ $1 = store-full ]
then
/usr/sbin/sendmail $SENDTO <<EOM
Subject: $SUBJECT (store full)
To: $SENDTO


The store account for rocio is full.

=============================
FILES ARE NOT BEING BACKED UP
=============================

Please adjust the limits on account 2 on server localhost.

EOM
elif [ $1 = read-error ]
then
/usr/sbin/sendmail $SENDTO <<EOM
Subject: $SUBJECT (read errors)
To: $SENDTO


Errors occured reading some files or directories for backup on rocio.

===================================
THESE FILES ARE NOT BEING BACKED UP
===================================

Check the logs on rocio for the files and directories which caused
these errors, and take appropraite action.

Other files are being backed up.

EOM
else
/usr/sbin/sendmail $SENDTO <<EOM
Subject: $SUBJECT (unknown)
To: $SENDTO


The backup daemon on rocio reported an unknown error.

==========================
FILES MAY NOT BE BACKED UP
==========================

Please check the logs on rocio.

EOM
fi
