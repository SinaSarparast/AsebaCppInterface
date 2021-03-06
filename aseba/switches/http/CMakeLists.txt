# asebahttp - a switch to bridge HTTP to Aseba
# 2014-12-01 David James Sherman <david dot sherman at inria dot fr>

# need libxml2 to read aesl files
find_package(LibXml2)
if (LIBXML2_FOUND)
	include_directories(${LIBXML2_INCLUDE_DIR})

	set(http_SRCS
		http.cpp
		main.cpp
	)
	set(http_MOCS
		http.h
	)
	
	add_executable(asebahttp ${http_SRCS} ${http_MOCS})
	
	target_link_libraries(asebahttp asebacompiler asebacommon ${LIBXML2_LIBRARIES} asebadashelplugins)

	if(HAS_ZEROCONF_SUPPORT)
		target_link_libraries(asebahttp asebazeroconf)
	endif()


	install(TARGETS asebahttp RUNTIME
		DESTINATION bin
	)

	add_library(asebahttphub ${http_SRCS})
	target_link_libraries(asebahttphub asebacompiler asebacommon asebadashelplugins ${LIBXML2_LIBRARIES} )
	set_target_properties(asebahttphub PROPERTIES VERSION ${LIB_VERSION_STRING} 
						    SOVERSION ${LIB_VERSION_MAJOR})

	if(HAS_ZEROCONF_SUPPORT)
		target_link_libraries(asebahttphub asebazeroconf)
	endif()


	install(TARGETS asebahttphub
			LIBRARY DESTINATION ${LIB_INSTALL_DIR} 
			ARCHIVE DESTINATION ${LIB_INSTALL_DIR} 
	)

	set (ASEBACORE_HDR_HTTP
		http.h
	)
	install(FILES ${ASEBACORE_HDR_HTTP}
		DESTINATION include/aseba/switches/http
	)

endif (LIBXML2_FOUND)
