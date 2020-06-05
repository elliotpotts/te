#!/usr/bin/env python3

class ParseError(Exception):
    pass

class Parser:
    def __init__(self, data):
        self.data = data

    def expect(self, lit):
        head = self.data[0:len(lit)]
        if (head == lit):
            self.data = self.data[len(lit):len(self.data)]
        else:
            raise ParseError("Bad expect!")

    def take_while(self, head):
        sofar = b""
        while head(self.data):
            sofar += self.data[0:1]
            self.data = self.data[1:len(self.data)]
        return sofar

    def skip(self, n):
        self.data = self.data[n:len(self.data)]

    def excursion(self, subparse):
        subparser = Parser(self, self.data)
        try:
            val = subparse(subparser)
            self.data = subparser.data
            return val
        except (ParseError):
            return None

p = Parser(open("text.orig", "rb").read())        
p.expect(b"\x75\xB1\xC0\xBA\x00\x00\x01\x00")
p.expect(b"\x5D\xE4\x07\x00\x3D\xE6\x07\x00")

while True:
    print(str(p.take_while(lambda s: s[0] != 0), "ISO-8859-1"))
    p.skip(1)
