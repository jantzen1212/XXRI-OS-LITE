# -*-makefile-*-
#
# XML::Parser for the host perl.  rules/pre/xxri.make exports PERL5LIB so the
# host perl (PTXdist's host-system-perl check and intltool) finds it.
#
HOST_PACKAGES-$(PTXCONF_HOST_PERL_XML_PARSER) += host-perl-xml-parser

HOST_PERL_XML_PARSER_VERSION	:= 2.47
HOST_PERL_XML_PARSER_SHA256	:= ad4aae643ec784f489b956abe952432871a622d4e2b5c619e8855accbfc4d1d8
HOST_PERL_XML_PARSER		:= XML-Parser-$(HOST_PERL_XML_PARSER_VERSION)
HOST_PERL_XML_PARSER_SUFFIX	:= tar.gz
HOST_PERL_XML_PARSER_URL	:= https://cpan.metacpan.org/authors/id/T/TO/TODDR/$(HOST_PERL_XML_PARSER).$(HOST_PERL_XML_PARSER_SUFFIX)
HOST_PERL_XML_PARSER_SOURCE	:= $(SRCDIR)/$(HOST_PERL_XML_PARSER).$(HOST_PERL_XML_PARSER_SUFFIX)
HOST_PERL_XML_PARSER_DIR	:= $(HOST_BUILDDIR)/$(HOST_PERL_XML_PARSER)
HOST_PERL_XML_PARSER_LICENSE	:= Artistic-1.0-Perl OR GPL-1.0-or-later

$(STATEDIR)/host-perl-xml-parser.prepare:
	@$(call targetinfo)
	@cd $(HOST_PERL_XML_PARSER_DIR) && perl Makefile.PL INSTALL_BASE=$(XXRI_HOST_PERL5) > /dev/null
	@$(call touch)

$(STATEDIR)/host-perl-xml-parser.compile:
	@$(call targetinfo)
	@$(MAKE) -C $(HOST_PERL_XML_PARSER_DIR) > /dev/null
	@$(call touch)

# straight into the host sysroot (perl's own installer, no DESTDIR staging)
$(STATEDIR)/host-perl-xml-parser.install:
	@$(call targetinfo)
	@$(MAKE) -C $(HOST_PERL_XML_PARSER_DIR) install > /dev/null
	@$(call touch)

# the host perl module check must see it
$(STATEDIR)/host-system-perl.prepare: $(STATEDIR)/host-perl-xml-parser.install.post

# vim: syntax=make
