"""RFC 6901 pointer construction."""


def join(pointer, part):
    return pointer + '/' + str(part).replace('~', '~0').replace('/', '~1')
