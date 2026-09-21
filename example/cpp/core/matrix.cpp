// @file matrix.cpp
// @brief Hardware-free usage of the wse::Matrix_ class, reported through the WSE stream logger.
//
// A usage violation - a non-square matrix handed to determinant() or tryInverse(), a left
// column count that does not match the right row count in operator* - throws
// std::invalid_argument, so the arithmetic is wrapped in one try block. A singular matrix is
// not a usage violation: it can arrive in well-formed data, so tryInverse() reports it as a
// failed wse::CoreResult carrying SingularMatrix instead of throwing. The result is never a
// silent NaN. The exit code says which arm ran: 1 an inverse that did not round-trip or a
// singular input, 2 a thrown operation. Build with the always-on Core component and link
// WSE::Core.

#include <wse/stew.h>
#include <wse/error/CoreError.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

//! Formats a matrix row by row for one stream-logger line.
//! Subscripting is [row][column], the same order the constructor takes, even though the
//! underlying storage class is addressed width-first.
std::string describe( const wse::float64_matrix& matrix_in )
{
    std::string text;
    for ( size_t row = 0; row < matrix_in.rows(); ++row )
    {
        text += "[ ";
        for ( size_t column = 0; column < matrix_in.cols(); ++column )
        {
            text += std::to_string( matrix_in[ row ][ column ] ) + " ";
        }
        text += "]";
    }
    return text;
}

} // namespace

int main()
{
    wse::registDefaultLog();

    try
    {
        // Row-major construction from rows, columns, and element data. Rows come first, so
        // this reads [ 4 7 / 2 6 ]; the vector is consumed row after row and any length other
        // than rows * columns throws rather than padding or truncating.
        const wse::float64_matrix matrix( 2, 2, std::vector< double >{ 4.0, 7.0, 2.0, 6.0 } );
        wse::WLog() << "matrix:" << describe( matrix );

        wse::WLog() << "determinant:" << matrix.determinant();
        wse::WLog() << "transpose:" << describe( matrix.transpose() );

        // A singular matrix is an expectable outcome, so it arrives as a failed CoreResult.
        const wse::CoreResult< wse::float64_matrix > inverted = matrix.tryInverse();
        if ( !inverted.succeeded() )
        {
            wse::WLog() << "ERROR: the matrix is singular:" << inverted.error().message();
            return 1;
        }
        const wse::float64_matrix inverse = inverted.value();
        wse::WLog() << "inverse:" << describe( inverse );

        // A round trip through the inverse must reproduce the identity matrix.
        const wse::float64_matrix product = matrix * inverse;
        const wse::float64_matrix identity = wse::float64_matrix::Identify( 2 );
        double difference = 0.0;
        for ( size_t row = 0; row < product.rows(); ++row )
        {
            for ( size_t column = 0; column < product.cols(); ++column )
            {
                difference += std::fabs( product[ row ][ column ] - identity[ row ][ column ] );
            }
        }
        wse::WLog() << "matrix * inverse:" << describe( product );

        if ( difference > 1.0e-9 )
        {
            wse::WLog() << "ERROR: inverse round trip failed, accumulated difference"
                        << difference;
            return 1;
        }
        wse::WLog() << "inverse round trip succeeded";
    }
    catch ( const std::exception& exception )
    {
        wse::WLog() << "ERROR: matrix operation threw:" << exception.what();
        return 2;
    }

    return 0;
}
