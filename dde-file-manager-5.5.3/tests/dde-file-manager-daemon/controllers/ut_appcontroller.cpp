/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhengyouge<zhengyouge@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
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

#include <gtest/gtest.h>
#include "controllers/appcontroller.h"

namespace {
class TestAppController : public testing::Test
{
public:
    void SetUp() override
    {
        std::cout << "start TestAppController";
        ctrl = new AppController;
    }

    void TearDown() override
    {
        std::cout << "end TestAppController";
        delete ctrl;
        ctrl = nullptr;
    }

public:
    AppController *ctrl = nullptr;
};
} // namespace

TEST_F(TestAppController, initTest)
{
    ctrl->initConnect();
    ctrl->initControllers();
}
