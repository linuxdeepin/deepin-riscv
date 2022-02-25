// Code generated by "dbusutil-gen em -type Manager"; DO NOT EDIT.

package main

import (
	"pkg.deepin.io/lib/dbusutil"
)

func (v *Manager) GetExportedMethods() dbusutil.ExportedMethods {
	return dbusutil.ExportedMethods{
		{
			Name:    "Query",
			Fn:      v.Query,
			InArgs:  []string{"hans"},
			OutArgs: []string{"pinyin"},
		},
		{
			Name:    "QueryList",
			Fn:      v.QueryList,
			InArgs:  []string{"hansList"},
			OutArgs: []string{"jsonStr"},
		},
	}
}
