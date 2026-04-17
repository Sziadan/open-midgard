if(NOT DEFINED RO_SRC OR NOT DEFINED RO_DST)
    message(FATAL_ERROR "BestEffortCopy.cmake requires RO_SRC and RO_DST.")
endif()

get_filename_component(_ro_dst_dir "${RO_DST}" DIRECTORY)
file(MAKE_DIRECTORY "${_ro_dst_dir}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${RO_SRC}" "${RO_DST}"
    RESULT_VARIABLE _ro_copy_result
    ERROR_VARIABLE _ro_copy_error
)

if(NOT _ro_copy_result EQUAL 0)
    string(STRIP "${_ro_copy_error}" _ro_copy_error)
    message(WARNING "Best-effort deploy copy skipped: ${RO_SRC} -> ${RO_DST}: ${_ro_copy_error}")
endif()