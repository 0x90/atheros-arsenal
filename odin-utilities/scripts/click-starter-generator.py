
import sys
if (len(sys.argv) != 2):
    print 'Usage:'
    print 'click-starter-generator.py <CLICK_PATH>'
    sys.exit(0)

CLICK_PATH = sys.argv[1]

print '''
#!/bin/bash

cd %s
./tools/click-align/click-align ../scripts/agent.click | ./userlevel/click &> ../scripts/odin.log
''' % (CLICK_PATH)

