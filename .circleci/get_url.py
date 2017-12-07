import json
import sys

def main():
  assert (len (sys.argv) == 3)
  items = json.load (open (sys.argv[2], 'r'))
  for item in items:
    if item['path'].find(sys.argv[1]) > -1:
      print (item['url'])
      exit(0)
  exit(1)

main()
