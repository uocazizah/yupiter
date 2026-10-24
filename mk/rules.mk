# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - compilation rules
#
# Every object carries a dependency file produced by "-MMD -MP", so a
# header change rebuilds exactly the translation units that include it.

$(OBJDIR)/%.o: %.cxx
	@mkdir -p $(dir $@)
	$(CXX) $(LIB_CXXFLAGS) $(LIB_CPPFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(LIB_ASFLAGS) -MMD -MP -c $< -o $@

-include $(ARCHIVE_OBJECTS:.o=.d) $(SHARED_OBJECTS:.o=.d)
