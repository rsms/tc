Changes
=======

0.7.2
-----

* Fixed missing storage for tc.Error

0.7.1
-----

* Branched from pytc into tc by Rasmus Andersson
* Total refactoring including layout
* Added Python 2.6 and 3.0 compatibility
* Added documentation

0.7.0
-----

* Added Python 2.4 compatibility. Thanks to Vernon Tang.
* Allow negitive values on optimize and tune methods of pytc.BDB and pytc.HDB. Thanks to Vernon Tang.
* removed unnecessarily links(libz and libm). Thanks to Vernon Tang.

0.6.0
-----

* supports BZIP options for HDB/BDB


0.5.0
-----

* added addint/adddouble for HDB/BDB
* raise KeyError instead of TCENOREC. Thanks to Hatem Nassrat.


0.4.0
-----

* removed compile errors on Tokyo Cabinet 1.3.19.


0.3.0
-----

* modified setup.py for darwin
* added TCBDB.range and TCBDB.rangefwm

0.2.0
-----

* added iterkeys/itervalues/iteritems for HDB/BDB
* bug fixed when spcified db[:] (thanks a2c)
* refactoring

0.1.0
-----

* first version
* hdb interface (thanks much for Shin ADACHI)
* bdb interface
