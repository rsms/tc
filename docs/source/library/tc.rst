tc
===========================================================

.. module:: tc

Tokyo Cabinet Python bindings


Classes
-------------------------------------------------

* :class:`HDB`
* :class:`BDB`
* :class:`BDBCursor`


.. class:: HDB

   Tokyo Cabinet Hash database

   .. method:: adddouble()

      Add a real number to a record in a hash database object.

   .. method:: addint()

      Add an integer to a record in a hash database object.

   .. method:: close()

      Close a hash database object.

   .. method:: copy()

      Copy the database file of a hash database object.

   .. method:: ecode()

      Get the last happened error code of a hash database object.

   .. method:: static errmsg()

      Get the message string corresponding to an error code.

   .. method:: fsiz()

      Get the size of the database file of a hash database object.

   .. method:: get()

      Retrieve a record in a hash database object.

   .. method:: iterinit()

      Initialize the iterator of a hash database object.

   .. method:: iternext()

      Get the next extensible objects of the iterator of a hash
      database object.

   .. method:: next()

      x.next() -> the next value, or raise StopIteration

   .. method:: open()

      Open a database file and connect a hash database object.

   .. method:: optimize()

      Optimize the file of a hash database object.

   .. method:: out()

      Remove a record of a hash database object.

   .. method:: path()

      Get the file path of a hash database object.

   .. method:: put()

      Store a record into a hash database object.

   .. method:: putasync()

      Store a record into a hash database object in asynchronous
      fashion.

   .. method:: putcat()

      Concatenate a value at the end of the existing record in a hash
      database object.

   .. method:: putkeep()

      Store a new record into a hash database object.

   .. method:: rnum()

      Get the number of records of a hash database object.

   .. method:: setmutex()

      Set mutual exclusion control of a hash database object for
      threading.

   .. method:: sync()

      Synchronize updated contents of a hash database object with the
      file and the device.

   .. method:: tune()

      Set the tuning parameters of a hash database object.

   .. method:: vanish()

      Remove all records of a hash database object.

   .. method:: vsiz()

      Get the size of the value of a record in a hash database object.







.. class:: BDB

   Tokyo Cabinet B+ tree database

   .. method:: adddouble()

      Add a real number to a record in a B+ tree database object.

   .. method:: addint()

      Add an integer to a record in a B+ tree database object.

   .. method:: close()

      Close a B+ tree database object.

   .. method:: copy()

      Copy the database file of a B+ tree database object.

   .. method:: curnew()

      Create a cursor object.

   .. method:: ecode()

      Get the last happened error code of a B+ tree database object.

   .. staticmethod:: errmsg()

      Get the message string corresponding to an error code.

   .. method:: fsiz()

      Get the size of the database file of a B+ tree database object.

   .. method:: get()

      Retrieve a record in a B+ tree database object. If the key of
      duplicated records is specified, the value of the first record
      is selected.

   .. method:: getlist()

      Retrieve records in a B+ tree database object.

   .. method:: open()

      Open a database file and connect a B+ tree database object.

   .. method:: optimize()

      Optimize the file of a B+ tree database object.

   .. method:: out()

      Remove a record of a B+ tree database object.

   .. method:: outlist()

      Remove records of a B+ tree database object.

   .. method:: path()

      Get the file path of a hash database object.

   .. method:: put()

      Store a record into a B+ tree database object.

   .. method:: putcat()

      Concatenate a value at the end of the existing record in a B+
      tree database object.

   .. method:: putdup()

      Store a record into a B+ tree database object with allowing
      duplication of keys.

   .. method:: putkeep()

      Store a new record into a B+ tree database object.

   .. method:: putlist()

      Store records into a B+ tree database object with allowing
      duplication of keys.

   .. method:: rnum()

      Get the number of records of a B+ tree database object.

   .. method:: setcache()

      Set the caching parameters of a B+ tree database object.

   .. method:: setcmpfunc()

      Set the custom comparison function of a B+ tree database object.

   .. method:: setmutex()

      Set mutual exclusion control of a B+ tree database object for
      threading.

   .. method:: sync()

      Synchronize updated contents of a B+ tree database object with
      the file and the device.

   .. method:: tranabort()

      Abort the transaction of a B+ tree database object.

   .. method:: tranbegin()

      Begin the transaction of a B+ tree database object.

   .. method:: trancommit()

      Commit the transaction of a B+ tree database object.

   .. method:: tune()

      Set the tuning parameters of a B+ tree database object.

   .. method:: vanish()

      Remove all records of a hash database object.

   .. method:: vnum()

      Get the number of records corresponding a key in a B+ tree
      database object.

   .. method:: vsiz()

      Get the size of the value of a record in a B+ tree database
      object.






.. class:: BDBCursor

   Tokyo Cabinet B+ tree database cursor

   .. method:: first()

      Move a cursor object to the first record.

   .. method:: jump()

      Move a cursor object to the front of records corresponding a
      key.

   .. method:: key()

      Get the key of the record where the cursor object is.

   .. method:: last()

      Move a cursor object to the last record.

   .. method:: next()

      x.next() -> the next value, or raise StopIteration

   .. method:: out()

      Delete the record where a cursor object is.

   .. method:: prev()

      Move a cursor object to the previous record.

   .. method:: put()

      Insert a record around a cursor object.

   .. method:: rec()

      Get the key and the value of the record where the cursor object
      is.

   .. method:: val()

      Get the value of the record where the cursor object is.


Exceptions
-------------------------------------------------

.. exception:: Error
