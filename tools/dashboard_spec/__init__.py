"""Bounded dashboard document validation."""

from .loader import load, loads


def validate(document):
    from .schema import validate as validate_document
    return validate_document(document)

__all__ = ['load', 'loads', 'validate']
