import sys,re
# park.py FILE FUNC "note": wrap the C definition of FUNC (with its leading comment) in NON_MATCHING
p,func,note=sys.argv[1],sys.argv[2],sys.argv[3]
s=open(p).read()
m=re.search(r'^[A-Za-z][^\n;]*\b%s\([^;{]*\) \{\n' % re.escape(func), s, re.M)
a=m.start(); b=s.index("\n}\n",a)+3
# leading comment directly above (allow one blank line)
head=s[:a]
cm=re.search(r'(/\*(?:[^*]|\*(?!/))*\*/)\n\n?$', head)
if cm:
    i=cm.start(); c=cm.group(1)
    c=c[:-2].rstrip()+"\n * "+note+" */\n"
else:
    i=a; c="/* "+note+" */\n"
s=s[:i]+"#ifdef NON_MATCHING\n"+c+s[a:b]+'#else\nINCLUDE_ASM("asm/us/nonmatchings/%s", %s);\n#endif\n' % (p.split('/')[-1][:-2], func)+s[b:]
open(p,'w').write(s)
