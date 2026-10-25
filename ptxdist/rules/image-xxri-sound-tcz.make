# -*-makefile-*-
#
# xxri-sound.tcz (x86_64): the sound modules from xxri-kernel (all of ALSA plus
# SOF, SoundWire, ACP and the HDA amps) and their firmware (xxri-sound-firmware).
# Replaces Tiny Core's alsa-modules-6.12.11-tinycore64.tcz; alsa.tcz.dep in
# config/cde64 points here, and tce-load runs depmod for it.
#
IMAGE_PACKAGES-$(PTXCONF_IMAGE_XXRI_SOUND_TCZ) += image-xxri-sound-tcz

IMAGE_XXRI_SOUND_TCZ		:= image-xxri-sound-tcz
IMAGE_XXRI_SOUND_TCZ_DIR	:= $(BUILDDIR)/$(IMAGE_XXRI_SOUND_TCZ)
IMAGE_XXRI_SOUND_TCZ_IMAGE	:= $(IMAGEDIR)/xxri-sound.tcz
IMAGE_XXRI_SOUND_TCZ_PKGS	:= $(XXRI_SOUND_TCZ_PKGS)

ifdef PTXCONF_IMAGE_XXRI_SOUND_TCZ
$(IMAGE_XXRI_SOUND_TCZ_IMAGE):
	@$(call targetinfo)
	@$(call world/image/env, IMAGE_XXRI_SOUND_TCZ) ptxd_make_xxri_tcz ""
	@$(call finish)
endif

# vim: syntax=make
