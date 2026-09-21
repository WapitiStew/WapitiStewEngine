package io.wapitistew.wse;

/** Immutable WSE runtime and optional-component information. */
public record RuntimeInfo(
        String version,
        int bindingAbiVersion,
        boolean hasXpt,
        boolean hasTmr,
        boolean hasOui,
        boolean hasGef,
        boolean hasIui,
        boolean hasVpj) {}
