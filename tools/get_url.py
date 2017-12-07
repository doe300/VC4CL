import json
import sys

def main():
  assert (len (sys.argv) == 2)
  items = json.load (open (sys.argv[1], 'r'))
  for item in items:
    if item['path'].find('libVC4CC.so') > -1:
      print (item['url'])
      exit(0)
  exit(1)

main()
