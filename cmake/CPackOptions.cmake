# CPack DEB — included from root CMakeLists.txt after all install() rules.
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME central-logger-app)
set(CPACK_PACKAGE_VENDOR "4M Technologies")
set(CPACK_PACKAGE_CONTACT "dev@local")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_amd64")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Central Logger — Modbus TCP data logger desktop client")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
# OFF: Qt deploy bundles optional SQL/ODBC plugins; shlibdeps fails on CI without libmysqlclient, libpq, …
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libxkbcommon0, libegl1, libgl1, libfontconfig1, libdbus-1-3"
)
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/central-logger")

include(CPack)
