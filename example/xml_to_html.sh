#!/bin/sh
cp $1 /Users/ha7netd/saveme
OUT=/Users/ha7netd/weather/index.html
java -jar xalan.jar org.apache.xalan.xslt.Process \
    -IN $1 -XSL /Users/ha7netd/xml_to_html.xsl -OUT $OUT.tmp \
  > /Users/ha7netd/doof 2>&1
if [ $? -eq 0 ]; then
    if [ -f $OUT ]; then
	mv $OUT $OUT.old
    fi
    mv $OUT.tmp $OUT
    chmod 0644 $OUT
    rm $1
    exit $?
else
    rm $1
    exit $?
fi
