#!/usr/bin/env python
# -*- coding: utf-8 -*-

import json
import sys

def main():
  assert (len (sys.argv) == 2)
  items = json.load (open (sys.argv[1], 'r'))

  for item in items:
    job_name = item['workflows']['job_name']
    build_status = item['status']
    branch = item['branch']
    if job_name == 'cross' and build_status == 'success' and branch == 'master':
      print (item['build_num'])
      exit(0)
  exit (1)

main()
