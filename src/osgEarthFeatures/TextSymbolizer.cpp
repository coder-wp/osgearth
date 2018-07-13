/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2016 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include <osgEarthFeatures/TextSymbolizer>
#include <osgEarthFeatures/Feature>
#include <osgEarth/Registry>

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;

TextSymbolizer::TextSymbolizer(const TextSymbol* symbol) :
_symbol( symbol )
{
    //nop
}

namespace
{
    // rounds "x" up to the next power of 2
    int nextPowerOf2(int x)
    {
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x+1;
    }
    
    // transcodes an OE encoding to an OSG encoding
    osgText::String::Encoding convertEncoding(TextSymbol::Encoding enc)
    {
        switch(enc)
        {
        case TextSymbol::ENCODING_ASCII: return osgText::String::ENCODING_ASCII; break;
        case TextSymbol::ENCODING_UTF8:  return osgText::String::ENCODING_UTF8; break;
        case TextSymbol::ENCODING_UTF16: return osgText::String::ENCODING_UTF16; break;
        case TextSymbol::ENCODING_UTF32: return osgText::String::ENCODING_UTF32; break;
        default: return osgText::String::ENCODING_UNDEFINED;
        }
    }
}

osgText::String::Encoding
TextSymbolizer::getEncoding() const
{
    return _symbol.valid() && _symbol->encoding().isSet() ?
        convertEncoding(_symbol->encoding().get()) :
        osgText::String::ENCODING_UNDEFINED;
}

void
TextSymbolizer::apply(osgText::Text* drawable,
                      Feature* feature,
                      const FilterContext* context,
                      const osg::BoundingBox* box_in) const
{
    if (!_symbol.valid())
        return;

    osgText::String::Encoding encoding = convertEncoding(_symbol->encoding().get());
    if (_symbol->content().isSet())
    {
        StringExpression temp(_symbol->content().get());
        std::string content = feature ? feature->eval(temp, context) : _symbol->content()->eval();
        drawable->setText(content, encoding);
    }

    // osgText::Text turns on depth writing by default, even if you turned it off.
    // GW: move this to osgEarth::Text?
    drawable->setEnableDepthWrites( false );

    if ( _symbol->layout().isSet() )
    {
        if(_symbol->layout().value() == TextSymbol::LAYOUT_RIGHT_TO_LEFT)
        {
            drawable->setLayout(osgText::TextBase::RIGHT_TO_LEFT);
        }
        else if(_symbol->layout().value() == TextSymbol::LAYOUT_LEFT_TO_RIGHT)
        {
            drawable->setLayout(osgText::TextBase::LEFT_TO_RIGHT);
        }
        else if(_symbol->layout().value() == TextSymbol::LAYOUT_VERTICAL)
        {
            drawable->setLayout(osgText::TextBase::VERTICAL);
        }
    }

    // calculate the text position relative to the alignment box.
    static osg::BoundingBox s_defaultbbox(0,0,0,0,0,0);
    const osg::BoundingBox& box = box_in? *box_in : s_defaultbbox;

    osg::Vec3f pos;

    osgText::Text::AlignmentType align = osgText::Text::CENTER_CENTER;
    // they're the same enum, but we need to apply the BBOX offsets.
    align = (osgText::Text::AlignmentType)_symbol->alignment().value();

    switch( align )
    {
    case osgText::Text::LEFT_TOP:
        pos.x() = box.xMax();
        pos.y() = box.yMin();
        break;
    case osgText::Text::LEFT_CENTER:
        pos.x() = box.xMax();
        pos.y() = box.center().y();
        break;
    case osgText::Text::LEFT_BOTTOM:
    case osgText::Text::LEFT_BOTTOM_BASE_LINE:
    case osgText::Text::LEFT_BASE_LINE:
        pos.x() = box.xMax();
        pos.y() = box.yMax();
        break;

    case osgText::Text::RIGHT_TOP:
        pos.x() = box.xMin();
        pos.y() = box.yMin();
        break;
    case osgText::Text::RIGHT_CENTER:
        pos.x() = box.xMin();
        pos.y() = box.center().y();
        break;
    case osgText::Text::RIGHT_BOTTOM:
    case osgText::Text::RIGHT_BOTTOM_BASE_LINE:
    case osgText::Text::RIGHT_BASE_LINE:
        pos.x() = box.xMin();
        pos.y() = box.yMax();
        break;

    case osgText::Text::CENTER_TOP:
        pos.x() = box.center().x();
        pos.y() = box.yMin();
        break;
    case osgText::Text::CENTER_BOTTOM:
    case osgText::Text::CENTER_BOTTOM_BASE_LINE:
    case osgText::Text::CENTER_BASE_LINE:
        pos.x() = box.center().x();
        pos.y() = box.yMax();
        break;
    case osgText::Text::CENTER_CENTER:
    default:
        pos = box.center();
        break;
    }

    drawable->setPosition( pos );

    drawable->setAlignment( align );

    // gw: move these to AnnotationUtils?
    drawable->setAutoRotateToScreen(false);
    drawable->setCullingActive(false);
    drawable->setCharacterSizeMode( osgText::Text::OBJECT_COORDS );
    
    float size = _symbol->size().isSet() ? (float)(_symbol->size()->eval()) : 16.0f;    

    drawable->setCharacterSize( size * Registry::instance()->getDevicePixelRatio() );

    drawable->setColor( _symbol->fill().isSet() ? _symbol->fill()->color() : Color::White );

    osg::ref_ptr<osgText::Font> font;
    if ( _symbol->font().isSet() )
    {
        font = osgText::readRefFontFile( *_symbol->font() );
    }

    if ( !font )
    {
        font = Registry::instance()->getDefaultFont();
    }

    if ( font )
    {
        drawable->setFont( font );
        
#if OSG_VERSION_LESS_THAN(3,5,8)
        // mitigates mipmapping issues that cause rendering artifacts for some fonts/placement
        font->setGlyphImageMargin( 2 );
#endif        
    }

#if OSG_VERSION_LESS_THAN(3,5,8)
    // OSG 3.4.x adds a program on the drawable itself,
    // so we remove it since we're using a VP on the font instead
    //drawable->setStateSet(0L);
#endif

#if OSG_VERSION_GREATER_OR_EQUAL(3,5,8)
    drawable->setShaderTechnique(osgText::ALL_FEATURES);
#endif

    float resFactor = 2.0f;

#if OSG_VERSION_LESS_THAN(3,5,8)
    int res = nextPowerOf2((int)(size*resFactor));
    drawable->setFontResolution(res, res);
#endif
    
    if ( _symbol->halo().isSet() )
    {
        // never worked right -gw
//#if OSG_VERSION_LESS_THAN(3,5,8)
        //float offsetFactor = 1.0f / (resFactor*256.0f);
        //drawable->setBackdropOffset(
        //    (float)drawable->getFontWidth() * offsetFactor,
        //    (float)drawable->getFontHeight() * offsetFactor);
//#endif

        drawable->setBackdropColor( _symbol->halo()->color() );

        if ( _symbol->haloBackdropType().isSet() )
        {
            drawable->setBackdropType( *_symbol->haloBackdropType() );
        }
        else
        {
            drawable->setBackdropType( osgText::Text::OUTLINE );
        }

#if OSG_VERSION_LESS_THAN(3,5,8)
        // deprecated since OSG 3.5.8
        if ( _symbol->haloImplementation().isSet() )
        {
            drawable->setBackdropImplementation( *_symbol->haloImplementation() );
        }
#endif

        if ( _symbol->haloOffset().isSet() )
        {
            drawable->setBackdropOffset( *_symbol->haloOffset(), *_symbol->haloOffset() );
        }
    }

    // this disables the default rendering bin set by osgText::Font. 
    // Necessary if we're going to do decluttering.
    // gw: consider moving this into annotationutils
    if ( drawable->getStateSet() )
    {
        drawable->getStateSet()->setRenderBinToInherit();
    }
}

void
TextSymbolizer::apply(osgText::Text* drawable) const
{
    return apply(drawable, 0L, 0L, 0L);
}
