"""Stable errors and chosen default limits, in bytes where applicable."""
from dataclasses import dataclass
from types import MappingProxyType

CODES = MappingProxyType({
    "E_JSON_SYNTAX": 1,
    "E_DUPLICATE_KEY": 2,
    "E_DOC_TOO_LARGE": 3,
    "E_JSON_TOO_DEEP": 4,
    "E_JSON_TOO_MANY_NODES": 5,
    "E_JSON_STRING_TOO_LONG": 6,
    "E_SCHEMA": 7,
    "E_UNRESOLVED_SIGNAL": 9,
    "E_UNRESOLVED_THEME_TOKEN": 10,
    "E_UNRESOLVED_PAGE": 11,
    "E_UNRESOLVED_PARENT": 12,
    "E_COMPONENT_CYCLE": 14,
    "E_NO_ROOT": 15,
    "E_MULTIPLE_ROOTS": 16,
    "E_ORPHAN_SUBTREE": 17,
    "E_ASPECT_POLICY_FORBIDDEN": 18,
    "E_IMAGE_NOT_IN_MANIFEST": 19,
    "E_ASSET_REFERENCE_FORBIDDEN": 20,
    "E_FIELD_PAST_FRAME_LENGTH": 21,
    "E_RULE_UNIT_MISMATCH": 23,
    "E_EXPRESSION_TOO_DEEP": 24,
    "E_EXPRESSION_TOO_MANY_NODES": 25,
    "E_TOO_MANY_COMPONENTS": 26,
    "E_TOO_MANY_HISTORY_SAMPLES": 27,
    "E_PKG_EXPANDED_SIZE": 28,
    "E_PKG_ENTRY_COUNT": 29,
    "E_PKG_ASSET_SIZE": 30,
    "E_PKG_PATH_ABSOLUTE": 31,
    "E_PKG_PATH_TRAVERSAL": 32,
    "E_PKG_PATH_DRIVE_LETTER": 33,
    "E_PKG_PATH_LINK": 34,
    "E_PKG_DUPLICATE_ENTRY": 35,
    "E_PKG_CASE_COLLISION": 36,
    "E_PKG_IMAGE_DIMENSIONS": 37,
    "E_PKG_IMAGE_HEADER_MISMATCH": 38,
    "E_PKG_IMAGE_FORMAT_UNSUPPORTED": 39,
    "E_PKG_TEXTURE_BUDGET": 40,
    "E_PKG_ZIP_MALFORMED": 41,
    "E_PKG_UNSUPPORTED_DOCUMENT": 42,
    "E_UNRESOLVED_COMPONENT": 43,
    "E_PKG_ASSET_NOT_IN_PACKAGE": 44,
    "E_PKG_ASSET_UNREADABLE": 45,
})

LIMITS = MappingProxyType({'expanded_size': 67108864, 'entry_count': 4096, 'asset_size': 16777216, 'document_size': 4194304, 'json_depth': 64, 'json_nodes': 200000, 'string_length': 65536, 'expression_depth': 32, 'expression_nodes': 20000, 'rule_nodes': 512, 'components': 2000, 'history_samples': 4096, 'image_dimension': 4096, 'mobile_texture': 201326592, 'desktop_texture': 536870912})

@dataclass
class Failure(Exception):
    code: str
    pointer: str
    message: str

    def __str__(self):
        return f"{self.code} {self.pointer}: {self.message}"
