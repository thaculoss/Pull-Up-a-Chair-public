"""Menu categories only; food records and authored house combos stay intact."""
SOUP_SOURCES={
    ('dragonborn.esm',0x03CD5B),
    ('hearthfires.esm',0x00353D),('hearthfires.esm',0x00353E),
    ('skyrim.esm',0x0EBA01),('skyrim.esm',0x0EBA02),
    ('skyrim.esm',0x0F4314),('skyrim.esm',0x0F4315),
    ('skyrim.esm',0x0F431B),('skyrim.esm',0x0F431C),
    ('skyrim.esm',0x0F431D),('skyrim.esm',0x0F431E),
}
def is_soup(plugin,local):return (plugin.lower(),local) in SOUP_SOURCES
