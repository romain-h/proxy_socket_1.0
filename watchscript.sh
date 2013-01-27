!/bin/bash
watchr -e 'watch("less/(.*)\.less$") {system "make && ./proxy 1234}'

