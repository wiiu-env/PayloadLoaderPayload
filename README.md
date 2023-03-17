# PayloadLoaderPayload
This payload.elf loader which can be used with any `payload.elf` loader. (For example [PayloadFromRPX](https://github.com/wiiu-env/PayloadFromRPX) or [JsTypeHax](https://github.com/wiiu-env/JsTypeHax))

## Usage
Place the `payload.elf` in the `sd:/wiiu` folder of your sd card and run a exploit which loads `payload.elf`.
Per default this will chainload `sd:/wiiu/payloads/default/payload.elf`, but when holding B while loading this payload, the target payload can be selected.

Place payloads into seperate folder in `sd:/wiiu/payloads/` example:

```
sd:/wiiu/payloads/default/payload.elf
sd:/wiiu/payloads/legacy_env/payload.elf
sd:/wiiu/payloads/fw_img_loader/payload.elf
```

If `sd:/wiiu/payloads/default/payload.elf` not present and user does NOT hold B while loading, no payload is loaded. This may lead to undefined behaviour on some payload.elf loaders.

## Building

For building you just need [wut](https://github.com/devkitPro/wut/) installed, then use the `make` command.

## Building using the Dockerfile

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t payloadloaderpayload-builder

# make 
docker run -it --rm -v ${PWD}:/project payloadloaderpayload-builder make

# make clean
docker run -it --rm -v ${PWD}:/project payloadloaderpayload-builder make clean
```

## Format the code via docker

`docker run --rm -v ${PWD}:/src ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./src -i`

## Credits
- Maschell
- orboditilt
- Copy pasted the solution for using wut header in .elf files from [RetroArch](https://github.com/libretro/RetroArch)