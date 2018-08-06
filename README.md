# OTA for Linux things

## Brief

Implementation of a small full image active/inactive OTA system for automatically
updating Linux things.

## Mechanism

Two complete copies of your firmware are stored somewhere on your thing.
When uboot starts it loads both of the images in to RAM and validates them.
Unless it's told otherwise it boots the newest image.

Updating the firmware is then just a case of replacing the inactive copy
with the new version of the firmware and rebooting. Uboot tells the kernel
which copy it booted so that the OTA application knows which copy should
be replaced.

## Requirements

### Software
* uboot
* thingymcconfig
* your firmware as a FIT image

### Hardware
* Enough storage for your firmware image twice, uboot and any other bits you need.
* Enough RAM to hold the two images during boot up.
* A GPIO to trigger booting the previous firmware in recovery situations.

## Device config

Usually in /etc/thingjp/ota

```
keys
|-- rsa.pub
stamp.json
```

## Firmware repo

### Layout
```
manifest.json
sig.json
myimage_0.fit
myimage_1.fit
...
```

### Manifest format

```json
{
	"serial": 0,
	"images": [
		{
			"uuid": "8ec32fe0-9bff-44bd-9f84-0b63088b1f13",
			"version": 0,
			"size": 0,
			"enabled": true,
			"tags": [
			],
			"signatures": [
				{
					"type": "rsa-sha256",
					"data": "xxx"
				}
			]
		}
	]
}
```

```json
[
	{
		"type": "rsa-sha256",
		"data": "xxx"
	}
]
```

## Hacking/Testing

```
modprobe nandsim id_bytes=01,53,03,01,10 parts=1024,1024
```

## Example uboot script

```
loadaddr1=0x8a000000
loadaddr2=0x8a700000
fit1off=0x200000
fit2off=0x900000
imagesz=0x700000

loadfit=sf read ${fitaddr} ${fitoff} ${imagesz}
getfitts=if fdt addr ${fitaddr}; then fdt get value ${tsvar} / timestamp; else setenv ${tsvar} 0; fi
loadfit1=setenv fitaddr ${loadaddr1}; setenv fitoff ${fit1off}; setenv tsvar fit1ts; run loadfit; run getfitts
loadfit2=setenv fitaddr ${loadaddr2}; setenv fitoff ${fit2off}; setenv tsvar fit2ts; run loadfit; run getfitts
chooseimage=if itest $fit2ts -gt $fit1ts; then setenv loadaddr ${loadaddr2}; setenv activepart ${fit2off}; else setenv loadaddr ${loadaddr1}; setenv activepart ${fit1off}; fi
bootcmd=sf probe;run loadfit1; run loadfit2; run chooseimage; setenv bootargs ota.part=$activepart; bootm ${loadaddr}'
```
