package=zeromq
$(package)_version=4.3.2
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=ebd7b5c830d6428956b67a0454a7f8cbed1de74b3b01e5c33c5378e22740f763
$(package)_patches=b3123a2fd1e77cbdceb5ee7a70e796063b5ee5b9.patch 87b81926aaaea70c84d5a5ea6eda982b3425ceeb.patch
$(package)_dependencies=sodium

define $(package)_set_vars
  $(package)_config_opts=--without-documentation --disable-shared --with-libsodium
  $(package)_config_opts_linux=--with-pic
  $(package)_config_opts_freebsd=--with-pic
  $(package)_cxxflags=-std=c++11
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/b3123a2fd1e77cbdceb5ee7a70e796063b5ee5b9.patch && \
  patch -p1 < $($(package)_patch_dir)/87b81926aaaea70c84d5a5ea6eda982b3425ceeb.patch && \
  ./autogen.sh
endef

define $(package)_config_cmds
  $($(package)_autoconf) AR_FLAGS=$($(package)_arflags)
endef

define $(package)_build_cmds
  $(MAKE) src/libzmq.la
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-libLTLIBRARIES install-includeHEADERS install-pkgconfigDATA
endef

define $(package)_postprocess_cmds
  rm -rf bin share
endef
