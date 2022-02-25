// Code generated by "dbusutil-gen em -type Graphic"; DO NOT EDIT.

package main

import (
	"pkg.deepin.io/lib/dbusutil"
)

func (v *Graphic) GetExportedMethods() dbusutil.ExportedMethods {
	return dbusutil.ExportedMethods{
		{
			Name:   "BlurImage",
			Fn:     v.BlurImage,
			InArgs: []string{"srcFile", "dstFile", "sigma", "numSteps", "format"},
		},
		{
			Name:   "ClipImage",
			Fn:     v.ClipImage,
			InArgs: []string{"srcFile", "dstFile", "x", "y", "w", "h", "format"},
		},
		{
			Name:   "CompositeImage",
			Fn:     v.CompositeImage,
			InArgs: []string{"srcFile", "compFile", "dstFile", "x", "y", "format"},
		},
		{
			Name:    "CompositeImageUri",
			Fn:      v.CompositeImageUri,
			InArgs:  []string{"srcDataUri", "compDataUri", "x", "y", "format"},
			OutArgs: []string{"resultDataUri"},
		},
		{
			Name:   "ConvertDataUriToImage",
			Fn:     v.ConvertDataUriToImage,
			InArgs: []string{"dataUri", "dstFile", "format"},
		},
		{
			Name:   "ConvertImage",
			Fn:     v.ConvertImage,
			InArgs: []string{"srcFile", "dstFile", "format"},
		},
		{
			Name:    "ConvertImageToDataUri",
			Fn:      v.ConvertImageToDataUri,
			InArgs:  []string{"imgfile"},
			OutArgs: []string{"dataUri"},
		},
		{
			Name:   "FillImage",
			Fn:     v.FillImage,
			InArgs: []string{"srcFile", "dstFile", "width", "height", "style", "format"},
		},
		{
			Name:   "FlipImageHorizontal",
			Fn:     v.FlipImageHorizontal,
			InArgs: []string{"srcFile", "dstFile", "format"},
		},
		{
			Name:   "FlipImageVertical",
			Fn:     v.FlipImageVertical,
			InArgs: []string{"srcFile", "dstFile", "format"},
		},
		{
			Name:    "GetDominantColorOfImage",
			Fn:      v.GetDominantColorOfImage,
			InArgs:  []string{"imgFile"},
			OutArgs: []string{"h", "s", "v"},
		},
		{
			Name:    "GetImageSize",
			Fn:      v.GetImageSize,
			InArgs:  []string{"imgFile"},
			OutArgs: []string{"width", "height"},
		},
		{
			Name:    "Hsv2Rgb",
			Fn:      v.Hsv2Rgb,
			InArgs:  []string{"h", "s", "v"},
			OutArgs: []string{"r", "g", "b"},
		},
		{
			Name:   "ResizeImage",
			Fn:     v.ResizeImage,
			InArgs: []string{"srcFile", "dstFile", "newWidth", "newHeight", "format"},
		},
		{
			Name:    "Rgb2Hsv",
			Fn:      v.Rgb2Hsv,
			InArgs:  []string{"r", "g", "b"},
			OutArgs: []string{"h", "s", "v"},
		},
		{
			Name:   "RotateImageLeft",
			Fn:     v.RotateImageLeft,
			InArgs: []string{"srcFile", "dstFile", "format"},
		},
		{
			Name:   "RotateImageRight",
			Fn:     v.RotateImageRight,
			InArgs: []string{"srcFile", "dstFile", "format"},
		},
		{
			Name:   "ThumbnailImage",
			Fn:     v.ThumbnailImage,
			InArgs: []string{"srcFile", "dstFile", "maxWidth", "maxHeight", "format"},
		},
	}
}
