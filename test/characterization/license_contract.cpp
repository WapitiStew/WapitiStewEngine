// @file engine/wse/test/characterization/license_contract.cpp
// @brief The license surfaces report failures through the LicenseResult contract.
// @details License loading gates the SDK's paid feature levels, so its failure reporting is a
//          contract callers branch on rather than a diagnostic detail. The test pins the wave-4
//          error surface (LEGACY-008): a missing key file is an operational failure carried by a
//          failed LicenseResult with the file-open code, a generated key round-trips through
//          save/load, an out-of-range writer enum is a usage violation reported with
//          std::invalid_argument, an out-of-period license fails verification with
//          LicenseExpired and leaves the process unlicensed, a well-formed in-period license
//          loads successfully, and a second load in the same process is a usage violation
//          reported with std::logic_error. The loaded-license state is process-global, so the
//          order below (failures first, one successful load, then the double-load check) is part
//          of the test design.

#include <wse/stew.h>

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void expect(const bool condition_in, const char* const message_in)
    {
        if (!condition_in)
        {
            std::cerr << "FAILED: " << message_in << '\n';
            ++failures;
        }
    }
}

int main(const int argc_in, char** const argv_in)
{
    if (argc_in != 3)
    {
        std::cerr << "FAILED: key and license scratch paths are required\n";
        return 1;
    }

    // Both scratch paths come from the build system. A file left by an earlier run would turn the
    // missing-file case into a successful read, so they are removed first.
    const std::string keyPath(argv_in[1]);
    const std::string licensePath(argv_in[2]);
    std::remove(keyPath.c_str());
    std::remove(licensePath.c_str());

    // A path with no file behind it is an operational failure the caller branches on, so it must
    // arrive as a failed LicenseResult with the file-open code rather than as an empty key.
    {
        const wse::LicenseResult< std::vector<std::uint8_t> > missing =
            wse::Licensekey::load("", keyPath);
        expect(
            !missing.succeeded() &&
                missing.error().code() == wse::eLicenseErrorCode::FileOpenFailed,
            "A missing key file reports FileOpenFailed");
    }

    // The generated key must come back through load() as the same six raw bytes; the key is the
    // encryption secret, so a lossy round trip would silently invalidate every written license.
    expect(wse::Licensekey::genrate("", keyPath).succeeded(), "Generating a key succeeds");
    const wse::LicenseResult< std::vector<std::uint8_t> > key = wse::Licensekey::load("", keyPath);
    expect(key.succeeded(), "Loading the generated key succeeds");
    if (!key.succeeded())
    {
        return 1;
    }
    expect(key.value().size() == 6U, "The generated key holds six bytes");

    // An enum value outside the writer's enumeration cannot describe a license; it is a usage
    // violation and must be rejected before anything is written.
    {
        wse::LicenseWriter::sConfig invalid;
        invalid.type = static_cast<wse::LicenseWriter::eLicenseType>(99);
        invalid.name = licensePath;
        bool rejectedType = false;
        try
        {
            (void)wse::LicenseWriter::save(key.value(), invalid);
        }
        catch (const std::invalid_argument&)
        {
            rejectedType = true;
        }
        expect(rejectedType, "An out-of-range license type is rejected");
    }

    // An expiration-date license whose window closed years ago must fail verification with the
    // expired code and must not mark the process as licensed - the later valid load proves that.
    {
        wse::LicenseWriter::sConfig expired;
        expired.type = wse::LicenseWriter::eLicenseType::ExpirationDate;
        expired.version = wse::LicenseWriter::eLicenseVersion::Stable;
        expired.start_year = 2019;
        expired.start_month = 1;
        expired.start_date = 1;
        expired.limit_year = 2020;
        expired.limit_month = 1;
        expired.limit_date = 1;
        expired.name = licensePath;
        expect(wse::LicenseWriter::save(key.value(), expired).succeeded(),
            "Writing the expired license succeeds");

        const wse::LicenseStatus loaded = wse::License::load(key.value(), "", licensePath);
        expect(
            !loaded.succeeded() &&
                loaded.error().code() == wse::eLicenseErrorCode::LicenseExpired,
            "An out-of-period license reports LicenseExpired");
    }

    // A well-formed license whose window is open loads successfully. This is the single
    // successful load the process gets, so it runs after every failure case above.
    {
        wse::LicenseWriter::sConfig valid;
        valid.type = wse::LicenseWriter::eLicenseType::ExpirationDate;
        valid.version = wse::LicenseWriter::eLicenseVersion::Stable;
        valid.masor = 1;
        valid.start_year = 2020;
        valid.start_month = 1;
        valid.start_date = 1;
        valid.limit_year = 2199;
        valid.limit_month = 12;
        valid.limit_date = 31;
        valid.name = licensePath;
        expect(wse::LicenseWriter::save(key.value(), valid).succeeded(),
            "Writing the valid license succeeds");
        expect(wse::License::load(key.value(), "", licensePath).succeeded(),
            "An in-period license loads successfully");
    }

    // The loaded license is process-global state; loading twice is a usage violation the caller
    // can avoid, so the contract is std::logic_error rather than a result.
    {
        bool rejectedSecondLoad = false;
        try
        {
            (void)wse::License::load(key.value(), "", licensePath);
        }
        catch (const std::logic_error&)
        {
            rejectedSecondLoad = true;
        }
        expect(rejectedSecondLoad, "A second license load is rejected");
    }

    std::remove(keyPath.c_str());
    std::remove(licensePath.c_str());
    return failures == 0 ? 0 : 1;
}
