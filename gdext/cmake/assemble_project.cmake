# Assembles the run copy of the Godot project under the build tree.
#
#   cmake -DSOURCE_DIR=<repo>/game -DPROJECT_DIR=<build>/game -P assemble_project.cmake
#
# Every entry of SOURCE_DIR (files and folders, except .godot and bin) gets a
# link of the same name in PROJECT_DIR, so the editor reads and saves the
# repository's files. PROJECT_DIR/bin is the extension's output directory
# and PROJECT_DIR/.godot is Godot's cache, both real folders in the build
# tree. Nothing is written into SOURCE_DIR.
#
# Links: symbolic links on Linux and macOS. On Windows, folders become
# directory junctions (no administrator rights or Developer Mode needed) and
# files symbolic links where allowed, else hard links (same volume), else
# copies, with a warning that edits to the copy do not reach the repository.

cmake_minimum_required(VERSION 3.25)

if(NOT SOURCE_DIR OR NOT PROJECT_DIR)
    message(FATAL_ERROR "assemble_project.cmake: SOURCE_DIR and PROJECT_DIR are required")
endif()
file(REAL_PATH "${SOURCE_DIR}" SOURCE_DIR)
cmake_path(IS_PREFIX SOURCE_DIR "${PROJECT_DIR}" NORMALIZE _inside)
if(_inside)
    message(FATAL_ERROR "assemble_project.cmake: the run copy (${PROJECT_DIR}) must not be inside ${SOURCE_DIR}")
endif()

file(MAKE_DIRECTORY "${PROJECT_DIR}/bin" "${PROJECT_DIR}/.godot")
# The library folder holds no resources; keep the editor from scanning it.
if(NOT EXISTS "${PROJECT_DIR}/bin/.gdignore")
    file(WRITE "${PROJECT_DIR}/bin/.gdignore" "")
endif()

set(_reserved .godot bin)

file(GLOB _entries LIST_DIRECTORIES true RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*" "${SOURCE_DIR}/.*")
list(REMOVE_DUPLICATES _entries)
list(SORT _entries)

foreach(_name IN LISTS _entries)
    if(_name IN_LIST _reserved OR _name STREQUAL "." OR _name STREQUAL "..")
        continue()
    endif()
    set(_src "${SOURCE_DIR}/${_name}")
    set(_dst "${PROJECT_DIR}/${_name}")

    if(IS_SYMLINK "${_dst}")
        file(READ_SYMLINK "${_dst}" _target)
        if(_target STREQUAL _src)
            continue()
        endif()
        file(REMOVE "${_dst}")
    elseif(EXISTS "${_dst}")
        if(IS_DIRECTORY "${_src}")
            # A junction (Windows) is not reported as a symlink; a folder that
            # resolves to the source folder is already linked.
            file(REAL_PATH "${_dst}" _real)
            if(_real STREQUAL _src)
                continue()
            endif()
            message(FATAL_ERROR "assemble_project.cmake: ${_dst} is a real folder, not a link to ${_src}. "
                "Delete it and build again.")
        endif()
        # A file: a hard link or copy from an earlier run, or a file the
        # editor replaced when saving (safe save renames a temporary file
        # over the link). Keep it if it equals the source, else refuse to
        # lose the edit.
        file(SHA256 "${_src}" _hs)
        file(SHA256 "${_dst}" _hd)
        if(NOT _hs STREQUAL _hd)
            message(WARNING "assemble_project.cmake: ${_dst} differs from ${_src}. It was edited in the run "
                "copy (for example project settings saved by the editor). Copy it to the repository if the "
                "change is wanted, then delete ${_dst} and build again.")
            continue()
        endif()
        if(WIN32)
            continue()
        endif()
        file(REMOVE "${_dst}")
    endif()

    if(IS_DIRECTORY "${_src}" AND WIN32)
        file(TO_NATIVE_PATH "${_dst}" _ndst)
        file(TO_NATIVE_PATH "${_src}" _nsrc)
        execute_process(COMMAND cmd /c mklink /J "${_ndst}" "${_nsrc}"
            RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "assemble_project.cmake: cannot create the junction ${_dst} -> ${_src}: ${_err}")
        endif()
    elseif(IS_DIRECTORY "${_src}")
        file(CREATE_LINK "${_src}" "${_dst}" SYMBOLIC)
    else()
        file(CREATE_LINK "${_src}" "${_dst}" SYMBOLIC RESULT _rc)
        if(NOT _rc EQUAL 0)
            file(CREATE_LINK "${_src}" "${_dst}" RESULT _rc)
        endif()
        if(NOT _rc EQUAL 0)
            file(COPY_FILE "${_src}" "${_dst}")
            message(WARNING "assemble_project.cmake: ${_dst} is a copy of ${_src} (no symbolic or hard link "
                "possible); edits made through the editor will not reach the repository.")
        endif()
    endif()
endforeach()

# Remove links whose source entry no longer exists.
file(GLOB _existing LIST_DIRECTORIES true RELATIVE "${PROJECT_DIR}" "${PROJECT_DIR}/*" "${PROJECT_DIR}/.*")
foreach(_name IN LISTS _existing)
    if(_name IN_LIST _reserved OR _name IN_LIST _entries)
        continue()
    endif()
    if(IS_SYMLINK "${PROJECT_DIR}/${_name}")
        file(REMOVE "${PROJECT_DIR}/${_name}")
    endif()
endforeach()

# Godot loads the extensions listed in .godot/extension_list.cfg. The editor
# writes this file when it scans the project; writing it here lets the
# viewer run straight after a build, before the editor has ever opened the
# run copy. One res:// path per line.
set(_list "${PROJECT_DIR}/.godot/extension_list.cfg")
set(_want "res://unmapped.gdextension\n")
set(_have "")
if(EXISTS "${_list}")
    file(READ "${_list}" _have)
endif()
if(NOT _have STREQUAL _want)
    file(WRITE "${_list}" "${_want}")
endif()
