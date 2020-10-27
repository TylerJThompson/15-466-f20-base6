#This makefile for Windows was originally made by Dominic Calkosz and made available in the class Discord.
#I did not write this makefile originally, I edited it for scenes besides hexapod and to run export-walkmeshes.py

all : \
    ../dist/game2-city.pnct \
    ../dist/game2-city.scene \
#    ../dist/game2-city.w \

../dist/game2-city.scene : game2-city.blend export-scene.py
    "C:\Program Files\Blender Foundation\Blender 2.90\blender.exe" --background --python export-scene.py -- "game2-city.blend:Main" "../dist/game2-city.scene"

../dist/game2-city.pnct : game2-city.blend export-meshes.py
    "C:\Program Files\Blender Foundation\Blender 2.90\blender.exe" --background --python export-meshes.py -- "game2-city.blend:Main" "../dist/game2-city.pnct"

#../dist/game2-city.w : game2-city.blend export-walkmeshes.py
#    "C:\Program Files\Blender Foundation\Blender 2.90\blender.exe" --background --python export-walkmeshes.py -- "game2-city.blend:WalkMeshes" "../dist/game2-city.w"