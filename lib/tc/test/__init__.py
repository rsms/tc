# encoding: utf-8
import unittest, sys, os

def suite():
  suites = []
  import hdb, bdb
  suites.append(hdb.suite())
  suites.append(bdb.suite())
  return unittest.TestSuite(suites)

def test(*va, **kw):
  runner = unittest.TextTestRunner(*va, **kw)
  return runner.run(suite())

if __name__ == "__main__":
  test()
