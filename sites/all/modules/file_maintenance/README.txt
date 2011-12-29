
README file for the File maintenance Drupal module.


Introduction
============

Ever wanted to move files from your sites/default/files directory (or whatever
you have configured) to a (new) subdirectory? Never did it because you knew it's
a damn mess since you need do move the physical file, adjust the database files
table and look through all the node_revisions table and other content to adjust
the link to the file so as to not break your site?
After numerous such requests of clients and searching for a module to do just
that, I got fed up about this huge Drupal core shortcoming, in my humble opinion,
and wrote File maintenance. 


What this module is not
=======================
- A replacement/extension for IMCE
- A replacement/extension for elFinder
- A replacement/extension for any file manager module out there


Then what is it?
================

It is constructed to be a bit like the Drupal core admin/content/node page. You
get an overview of all the directories in Drupal's files table. Note those words:
files table! File mantenance starts by scanning your files table and builds its
management page from there. If you have files in Drupal's files directory that
are not in the files table, they are not picked up! However if you have files
present in the files table that are not on disk, they will be noticed and are
highlighted in red.

Per directory found, the files inside it will be listed. You can sort the list
or filter it by entering (part of) the file name (using * as a wildcard) in the
filter field.
You can mark the files you wish to act upon and choose the appropriate bulk
action which will start a batch process to act on all the files you have
selected. The batch process will act on every selected file separately,
modifying the file on disk, the reference in the files table and search and
replace any textual path reference in the node_revisions table so as not to
break any HTML content referring to the old path of the file. You can add any
table and field to search in using the advanced settings pane on the settings
page.

When the bulk action is finished, you will be presented with a report of all
files processed. The data of this report resides in the cache table and will
stay there indefinitely. Unless you clear the cache tables ;-)

NOTE: File maintenance modifies your database in a number of ways during the
batch process, so you better have a database backup at hand!


Why would I use File maintenance
================================

- If you have a huge amount of files in your sites/default/files directory
  causing IMCE to be slooooooooow and want to clean up the mess.
- If you simply want to clean up the files directory and/or it's subdirectories.
- If you want to _migrate_ all of the files when changing the File system
  settings under admin/settings/file-system
- Any other reason you can think of to tinker with the files table ;-)


Installation
============

1. Extract the file_maintenance module and place it in
   /sites/all/modules/contrib

2. Enable the module and check out the settings on
   admin/settings/file-maintenance

   The interface can be found on admin/content/file-maintenance and the reports
   are located at admin/reports/file-maintenance

   File maintenance adds a new option to the Drupal core
   admin/settings/file-system page called 'Migrate files'. When you change your
   files directory using this form, you can use the 'Migrate files' button,
   instead of the regular save button, to move all of the files present in the
   files table to the new folder, keeping the directory structure and making
   sure that the links in the content tables are adjusted as well.

3. Give users that need to use this module proper the proper permissions:
    admins: administer file maintenance
    advanced users: access file maintenance


Support
=======

For support requests, bug reports, and feature requests, please us the issue cue
of File maintenance on http://drupal.org/project/issues/file_maintenance.

