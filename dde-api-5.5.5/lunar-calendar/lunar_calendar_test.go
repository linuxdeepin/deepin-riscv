/*
 * Copyright (C) 2019 ~ 2021 Uniontech Software Technology Co.,Ltd
 *
 * Author:     dengbo <dengbo@uniontech.com>
 *
 * Maintainer: dengbo <dengbo@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package main

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/suite"
	"pkg.deepin.io/lib/dbusutil"
)

type UnitTestSuite struct {
	suite.Suite
	m *Manager
}

func (s *UnitTestSuite) SetupSuite() {
	var err error
	s.m = &Manager{}
	s.m.service, err = dbusutil.NewSessionService()
	if err != nil {
		s.T().Skip(fmt.Sprintf("failed to get service: %v", err))
	}
}

func (s *UnitTestSuite) Test_GetInterfaceName() {
	s.m.GetInterfaceName()
}

func (s *UnitTestSuite) Test_GetLunarInfoBySolar() {
	_, _, err := s.m.GetLunarInfoBySolar(2021, 10, 1)
	s.Require().Nil(err)
}

func (s *UnitTestSuite) Test_GetFestivalsInRange() {
	_, err := s.m.GetFestivalsInRange("2021-01-02", "2021-10-01")
	s.Require().Nil(err)
}

func (s *UnitTestSuite) Test_GetLunarMonthCalendar() {
	_, _, err := s.m.GetLunarMonthCalendar(2021, 10, true)
	s.Require().Nil(err)
}

func (s *UnitTestSuite) Test_GetHuangLiDay() {
	_, err := s.m.GetHuangLiDay(2021, 10, 1)
	s.Require().Nil(err)
}

func (s *UnitTestSuite) Test_GetHuangLiMonth() {
	_, err := s.m.GetHuangLiMonth(2021, 10, true)
	s.Require().Nil(err)
}

func TestUnitTestSuite(t *testing.T) {
	suite.Run(t, new(UnitTestSuite))
}
