    if (a->flags & ASN1_OBJECT_FLAG_DYNAMIC_STRINGS) {
#ifndef CONST_STRICT            /* disable purely for compile-time strict
                                 * const checking. Doing this on a "real"
                                 * compile will cause memory leaks */
        OPENSSL_free((void*)a->sn);
        OPENSSL_free((void*)a->ln);
#endif
        a->sn = a->ln = NULL;
