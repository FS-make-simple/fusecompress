## Current Style ##

The coding style currently in use in FuseCompress is roughly:

  * Tab indentation, tab size 8
  * braces get their own line
  * both `//` and `/* */`-style comments are allowed
  * space after if, switch, while

You can get a good approximation using indent like so:

```
indent -bad -bap -nbbb -bl -bls -cdw -ce -cli8 -ts8 -i8 -l112 -lc112 -ut -bli0 -npsl -npcs -nce -brs
```

Don't blindly trust it, though, it is not very clever and lacks some features, too.

## New Style ##

I advise to use original Berkeley coding style for new or completely rewritten source files. Existing code should be migrated as it is being touched.