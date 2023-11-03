Here's what a wall tileset looks like. For reference see this RPG Codex
post:

- <<https://rpgcodex.net/forums/threads/codexian-game-development-thread.69297/post-8726752>>
- <<https://archive.ph/uthRB>>

Note: all textures in `floormat` are flat and unprojected. The renderer
projects then and adds any necessary effects.

Notes on textures:

- Can specify various seam modes.
  1. `"any"`: tiles are placed in arbitrary order.
  2. `"seamless"`: texture is made seamless from left to right, and
     top to bottom.
- Can have a tint specified for it. The formula is $a C + m$. Tint is
  useful to put a bit of contrast between North and West walls.
- Split into N tiles, width of 64 and height of 192 is used.
- An entire set of textures can be borrowed from a different rotation,
  either optionally mirrored and with a tint added on top of it.
- Can have multiple Z levels (in Tiled parlance it's "layers"), with
  optional Z = -1 layer for foundations/ground/soil. Levels are repeated
  if there's more Z levels than the tileset has.

For all NESW rotations the following separate textures are available:

1. Wall texture. Split into N tiles to be used on ingame 64x64x192 tiles.
   - The wall can be "borrowed" from a different rotation, optionally 
2. Wall's "depth" texture which specifies that the wall isn't paper-thin
   depending on its height. Split into N variants, typically N can be 1
   or the same amount as wall textures.
3. Inner wall layer. This can be paint, wallpaper or decals placed on
   top of the wall proper. A separate depth layer cannot be specified.
   Can be partly transparent (decals such as grafitti) or translucent
   (weathering marks made out of Perlin noise, etc).
5. Corner decorations for beginning and end of the wall. For a right
   angle of N+W walls, end decoration of N and beginning decoration of W
   is used.
6. Wall's side when the wall simply ends without forming a corner with a
   wall from another rotation.
7. Cut-out part for windows. With a stencil mask.
8. Cut-out part for doors without a frame. With a stencil mask.

Each texture set can be borrowed from another rotation, 

What about coordinates and which tile variant is picked? TODO

JSON/Lisp format. TODO
