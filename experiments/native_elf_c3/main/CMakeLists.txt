idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "../../../components/elf_loader/include"
                       REQUIRES elf_loader native_exec_pool)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../module/native.app.elf" BINARY)
