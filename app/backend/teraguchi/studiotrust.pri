# Generate a dependency-tracked header so retained builds cannot retain an old
# verification key when build inputs change. Empty input disables signed setup.
TERAGUCHI_STUDIO_KEY = $$(PLANK_STUDIO_CONFIG_PUBLIC_KEY)
!isEmpty(TERAGUCHI_STUDIO_KEY) {
    !contains(TERAGUCHI_STUDIO_KEY, "^[0-9a-f]{64}$"): error(Invalid studio verification key)
}
studio_key_header = $$OUT_PWD/teraguchi-studio-key.h
studio_key_line = "$${LITERAL_HASH}define TERAGUCHI_STUDIO_KEY_HEX \"$$TERAGUCHI_STUDIO_KEY\""
!write_file($$studio_key_header, studio_key_line): error(Cannot write studio verification header)
INCLUDEPATH += $$OUT_PWD
DEFINES += TERAGUCHI_STUDIO_KEY_BUILD
