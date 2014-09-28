// Copyright 2010-2014, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.model.SymbolMinorCategory;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.DescriptionLayoutPolicy;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.ValueScalingPolicy;
import org.mozc.android.inputmethod.japanese.ui.ScrollGuideView;
import org.mozc.android.inputmethod.japanese.ui.SpanFactory;
import org.mozc.android.inputmethod.japanese.ui.SymbolCandidateLayouter;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.RoundRectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import org.mozc.android.inputmethod.japanese.view.SymbolMajorCategoryButtonDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.TabSelectedBackgroundDrawable;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.support.v4.view.ViewPager.OnPageChangeListener;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.GestureDetector.OnGestureListener;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TabHost;
import android.widget.TabHost.OnTabChangeListener;
import android.widget.TabHost.TabSpec;
import android.widget.TabWidget;
import android.widget.TextView;

import java.util.List;

/**
 * This class is used to show symbol input view on which an user can
 * input emoticon/symbol/emoji.
 *
 * In this class,
 * "Emoticon" means "Kaomoji", like ＼(^o^)／
 * "Symbol" means symbol character, like $ % ! €
 * "Emoji" means a more graphical character of face, building, food etc.
 *
 * This class treats all Emoticon, Symbol and Emoji category as "SymbolMajorCategory".
 * A major category has several minor categories.
 * Each minor category belongs to only one major category.
 * Major-Minor relation is defined by using R.layout.symbol_minor_category_*.
 *
 */
public class SymbolInputView extends InOutAnimatedFrameLayout implements MemoryManageable {

  /**
   * Adapter for symbol candidate selection.
   * Exposed as package private for testing.
   */
  // TODO(hidehiko): make this class static.
  class SymbolCandidateSelectListener implements CandidateSelectListener {
    @Override
    public void onCandidateSelected(CandidateWord candidateWord) {
      if (viewEventListener != null) {
        // If we are on password field, history shouldn't be updated to protect privacy.
        viewEventListener.onSymbolCandidateSelected(
            currentMajorCategory, candidateWord.getValue(), !isPasswordField);
      }
    }
  }

  /**
   * Click handler of major category buttons.
   */
  class MajorCategoryButtonClickListener implements OnClickListener {
    private final SymbolMajorCategory majorCategory;

    MajorCategoryButtonClickListener(SymbolMajorCategory majorCategory) {
      if (majorCategory == null) {
        throw new NullPointerException("majorCategory should not be null.");
      }
      this.majorCategory = majorCategory;
    }

    @Override
    public void onClick(View majorCategorySelectorButton) {
      if (viewEventListener != null) {
        viewEventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
      }

      if (emojiEnabled
          && majorCategory == SymbolMajorCategory.EMOJI
          && emojiProviderType == EmojiProviderType.NONE) {
        // Ask the user which emoji provider s/he'd like to use.
        // If the user cancels the dialog, do nothing.
        maybeInitializeEmojiProviderDialog(getContext());
        if (emojiProviderDialog != null) {
          IBinder token = getWindowToken();
          if (token != null) {
            MozcUtil.setWindowToken(token, emojiProviderDialog);
          } else {
            MozcLog.w("Unknown window token.");
          }

          // If a user selects a provider, the dialog handler will set major category
          // to EMOJI automatically. If s/he cancels, nothing will be happened.
          emojiProviderDialog.show();
        }
        return;
      }

      setMajorCategory(majorCategory);
    }
  }

  // Manages the relationship between.
  static class SymbolTabWidgetViewPagerAdapter extends PagerAdapter
      implements OnTabChangeListener, OnPageChangeListener {

    private static final int HISTORY_INDEX = 0;

    private final Context context;
    private final SymbolCandidateStorage symbolCandidateStorage;
    private final ViewEventListener viewEventListener;
    private final CandidateSelectListener candidateSelectListener;
    private final SymbolMajorCategory majorCategory;
    private final SkinType skinType;
    private final EmojiProviderType emojiProviderType;
    private final TabHost tabHost;
    private final ViewPager viewPager;
    private final float candidateTextSize;
    private final float descriptionTextSize;

    private View historyViewCache = null;
    private int scrollState = ViewPager.SCROLL_STATE_IDLE;

    SymbolTabWidgetViewPagerAdapter(
        Context context, SymbolCandidateStorage symbolCandidateStorage,
        ViewEventListener viewEventListener, CandidateSelectListener candidateSelectListener,
        SymbolMajorCategory majorCategory,
        SkinType skinType, EmojiProviderType emojiProviderType,
        TabHost tabHost, ViewPager viewPager,
        float candidateTextSize, float descriptionTextSize) {
      Preconditions.checkNotNull(emojiProviderType);

      this.context = context;
      this.symbolCandidateStorage = symbolCandidateStorage;
      this.viewEventListener = viewEventListener;
      this.candidateSelectListener = candidateSelectListener;
      this.majorCategory = majorCategory;
      this.skinType = skinType;
      this.emojiProviderType = emojiProviderType;
      this.tabHost = tabHost;
      this.viewPager = viewPager;
      this.candidateTextSize = candidateTextSize;
      this.descriptionTextSize = descriptionTextSize;
    }

    private void maybeResetHistoryView() {
      if (viewPager.getCurrentItem() != HISTORY_INDEX && historyViewCache != null) {
        resetHistoryView();
      }
    }

    private void resetHistoryView() {
      CandidateList candidateList =
          symbolCandidateStorage.getCandidateList(majorCategory.minorCategories.get(0));
      if (candidateList.getCandidatesCount() == 0) {
        historyViewCache.findViewById(R.id.symbol_input_no_history).setVisibility(View.VISIBLE);
      } else {
        historyViewCache.findViewById(R.id.symbol_input_no_history).setVisibility(View.GONE);
      }
      SymbolCandidateView.class.cast(
          historyViewCache.findViewById(R.id.symbol_input_candidate_view)).update(candidateList);
    }

    @Override
    public void onPageScrollStateChanged(int state) {
      if (scrollState == ViewPager.SCROLL_STATE_IDLE) {
        maybeResetHistoryView();
      }
      scrollState = state;
    }

    @Override
    public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
      // Do nothing.
    }

    @Override
    public void onPageSelected(int position) {
      tabHost.setOnTabChangedListener(null);
      tabHost.setCurrentTab(position);
      tabHost.setOnTabChangedListener(this);

      if (viewEventListener != null) {
        viewEventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
      }
    }

    @Override
    public void onTabChanged(String tabId) {
      int position = Integer.parseInt(tabId);

      if (position == HISTORY_INDEX) {
        maybeResetHistoryView();
      }
      viewPager.setCurrentItem(position, false);

      if (viewEventListener != null) {
        viewEventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
      }
    }

    @Override
    public int getCount() {
      return majorCategory.minorCategories.size();
    }

    @Override
    public boolean isViewFromObject(View view, Object item) {
      return view == item;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
      LayoutInflater inflater = LayoutInflater.from(context);
      inflater = inflater.cloneInContext(context);

      View view = MozcUtil.inflateWithOutOfMemoryRetrial(
          View.class, inflater, R.layout.symbol_candidate_view, Optional.<ViewGroup>absent(),
          false);
      SymbolCandidateView symbolCandidateView =
          SymbolCandidateView.class.cast(view.findViewById(R.id.symbol_input_candidate_view));
      symbolCandidateView.setCandidateSelectListener(candidateSelectListener);
      symbolCandidateView.setMinColumnWidth(
          context.getResources().getDimension(majorCategory.minColumnWidthResourceId));
      symbolCandidateView.setSkinType(skinType);
      symbolCandidateView.setEmojiProviderType(emojiProviderType);
      symbolCandidateView.setCandidateTextDimension(candidateTextSize, descriptionTextSize);

      // Set candidate contents.
      if (position == HISTORY_INDEX) {
        historyViewCache = view;
        resetHistoryView();
      } else {
        symbolCandidateView.update(symbolCandidateStorage.getCandidateList(
            majorCategory.minorCategories.get(position)));
        symbolCandidateView.updateScrollPositionBasedOnFocusedIndex();
      }

      ScrollGuideView scrollGuideView =
          ScrollGuideView.class.cast(view.findViewById(R.id.symbol_input_scroll_guide_view));
      scrollGuideView.setSkinType(skinType);

      // Connect guide and candidate view.
      scrollGuideView.setScroller(symbolCandidateView.scroller);
      symbolCandidateView.setScrollIndicator(scrollGuideView);

      container.addView(view);
      return view;
    }

    @Override
    public void destroyItem(ViewGroup collection, int position, Object view) {
      if (position == HISTORY_INDEX) {
        historyViewCache = null;
      }
      collection.removeView(View.class.cast(view));
    }
  }

  /**
   * The text view for the minor category tab.
   * The most thing is as same as base TextView, but if the text is too long to fit in
   * the view, this view automatically scales the text horizontally. (If there is enough
   * space, doesn't widen.)
   */
  private static class TabTextView extends TextView {

    /** Cached Paint instance to measure the text. */
    private final Paint paint = new Paint();

    TabTextView(Context context) {
      super(context);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
      super.onLayout(changed, left, top, right, bottom);
      resetTextXSize();
    }

    private void resetTextXSize() {
      // Reset the paint instance.
      Paint paint = this.paint;
      paint.reset();
      paint.setAntiAlias(true);
      paint.setTextSize(getTextSize());
      paint.setTypeface(getTypeface());

      // Measure the width for each line.
      CharSequence text = getText().toString();
      float maxTextWidth = 0;
      int beginIndex = 0;
      for (int i = beginIndex; i < text.length(); ++i) {
        if (text.charAt(i) == '\n') {
          // Split the line.
          float textWidth = paint.measureText(text, beginIndex, i);
          if (textWidth > maxTextWidth) {
            maxTextWidth = textWidth;
          }
          // Exclude '\n'.
          beginIndex = i + 1;
        }
      }
      {
        // Last line.
        float textWidth = paint.measureText(text, beginIndex, text.length());
        if (textWidth > maxTextWidth) {
          maxTextWidth = textWidth;
        }
      }

      // Calculate scale factory. Note that 0.98f is the heuristic value,
      // in order to avoid wrapping lines by sub px calculations just in case.
      float scaleX = (getWidth() - getPaddingLeft() - getPaddingRight()) * 0.98f / maxTextWidth;

      // Cap the scaleX by 1f not to widen.
      setTextScaleX(Math.min(scaleX, 1f));
    }
  }

  /**
   * An event listener for the menu dialog window.
   */
  private class EmojiProviderDialogListener implements DialogInterface.OnClickListener {
    private final Context context;

    EmojiProviderDialogListener(Context context) {
      this.context = context;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
      String value;
      TypedArray typedArray =
          context.getResources().obtainTypedArray(R.array.pref_emoji_provider_type_values);
      try {
        value = typedArray.getString(which);
      } finally {
        typedArray.recycle();
      }
      sharedPreferences.edit()
          .putString(PreferenceUtil.PREF_EMOJI_PROVIDER_TYPE, value)
          .commit();
      setMajorCategory(SymbolMajorCategory.EMOJI);
    }
  }

  /**
   * The candidate view for SymbolInputView.
   *
   * The differences from CandidateView.CandidateWordViewForConversion are
   * 1) this class scrolls horizontally 2) the layout algorithm is simpler.
   */
  static class SymbolCandidateView extends CandidateWordView {
    private static final String DESCRIPTION_DELIMITER = "\n";

    private View scrollGuideView = null;
    private GestureDetector gestureDetector = null;

    public SymbolCandidateView(Context context) {
      super(context, Orientation.VERTICAL);
    }

    public SymbolCandidateView(Context context, AttributeSet attributeSet) {
      super(context, attributeSet, Orientation.VERTICAL);
    }

    public SymbolCandidateView(Context context, AttributeSet attributeSet, int defaultStyle) {
      super(context, attributeSet, defaultStyle, Orientation.VERTICAL);
    }

    // Shared instance initializer.
    {
      setBackgroundDrawableType(DrawableType.SYMBOL_CANDIDATE_BACKGROUND);
      Resources resources = getResources();
      scroller.setDecayRate(
          resources.getInteger(R.integer.symbol_input_scroller_velocity_decay_rate) / 1000000f);
      scroller.setMinimumVelocity(
          resources.getInteger(R.integer.symbol_input_scroller_minimum_velocity));
      layouter = new SymbolCandidateLayouter();
    }

    void setCandidateTextDimension(float textSize, float descriptionTextSize) {
      Preconditions.checkArgument(textSize > 0);
      Preconditions.checkArgument(descriptionTextSize > 0);

      Resources resources = getResources();

      float valueHorizontalPadding =
          resources.getDimension(R.dimen.candidate_horizontal_padding_size);
      float descriptionHorizontalPadding =
          resources.getDimension(R.dimen.symbol_description_right_padding);
      float descriptionVerticalPadding =
          resources.getDimension(R.dimen.symbol_description_bottom_padding);

      candidateLayoutRenderer.setValueTextSize(textSize);
      candidateLayoutRenderer.setValueHorizontalPadding(valueHorizontalPadding);
      candidateLayoutRenderer.setValueScalingPolicy(ValueScalingPolicy.UNIFORM);
      candidateLayoutRenderer.setDescriptionTextSize(descriptionTextSize);
      candidateLayoutRenderer.setDescriptionHorizontalPadding(descriptionHorizontalPadding);
      candidateLayoutRenderer.setDescriptionVerticalPadding(descriptionVerticalPadding);
      candidateLayoutRenderer.setDescriptionLayoutPolicy(DescriptionLayoutPolicy.OVERLAY);

      SpanFactory spanFactory = new SpanFactory();
      spanFactory.setValueTextSize(textSize);
      spanFactory.setDescriptionTextSize(descriptionTextSize);
      spanFactory.setDescriptionDelimiter(DESCRIPTION_DELIMITER);

      SymbolCandidateLayouter layouter = SymbolCandidateLayouter.class.cast(this.layouter);
      layouter.setSpanFactory(spanFactory);
      layouter.setRowHeight(resources.getDimensionPixelSize(R.dimen.symbol_view_candidate_height));
    }

    @Override
    SymbolCandidateLayouter getCandidateLayouter() {
      return SymbolCandidateLayouter.class.cast(super.getCandidateLayouter());
    }

    void setMinColumnWidth(float minColumnWidth) {
      getCandidateLayouter().setMinColumnWidth(minColumnWidth);
      updateLayouter();
    }

    void setOnGestureListener(OnGestureListener gestureListener) {
      if (gestureListener == null) {
        gestureDetector = null;
      } else {
        gestureDetector = new GestureDetector(getContext(), gestureListener);
      }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
      if (gestureDetector != null && gestureDetector.onTouchEvent(event)) {
        return true;
      }
      return super.onTouchEvent(event);
    }

    @Override
    protected void onDraw(Canvas canvas) {
      super.onDraw(canvas);
      if (scrollGuideView != null) {
        scrollGuideView.invalidate();
      }
    }

    void setScrollIndicator(View scrollGuideView) {
      this.scrollGuideView = scrollGuideView;
    }
  }

  private class OutAnimationAdapter extends AnimationAdapter {
    @Override
    public void onAnimationEnd(Animation animation) {
      // Releases candidate resources. Also, on some devices, this cancels repeating invalidation
      // to support emoji related stuff.
      TabHost tabHost = getTabHost();
      tabHost.setOnTabChangedListener(null);
      ViewPager candidateViewPager = getCandidateViewPager();
      candidateViewPager.setAdapter(null);
      candidateViewPager.setOnPageChangeListener(null);
    }
  }

  /**
   * Name to represent this view for logging.
   */
  static final KeyboardSpecificationName SPEC_NAME =
      new KeyboardSpecificationName("SYMBOL_INPUT_VIEW", 0, 1, 0);
  // Source ID of the delete button for logging usage stats.
  private static final int DELETE_BUTTON_SOURCE_ID = 1;

  // TODO(hidehiko): move these parameters to skin instance.
  private static final float BUTTON_CORNOR_RADIUS = 3.5f;  // in dip.
  private static final float BUTTON_LEFT_OFFSET = 2.0f;
  private static final float BUTTON_TOP_OFFSET = 2.0f;
  private static final float BUTTON_RIGHT_OFFSET = 2.0f;
  private static final float BUTTON_BOTTOM_OFFSET = 2.0f;

  private static final int MAJOR_CATEGORY_TOP_COLOR = 0xFFF5F5F5;
  private static final int MAJOR_CATEGORY_BOTTOM_COLOR = 0xFFD2D2D2;
  private static final int MAJOR_CATEGORY_PRESSED_TOP_COLOR = 0xFFAAAAAA;
  private static final int MAJOR_CATEGORY_PRESSED_BOTTOM_COLOR = 0xFF828282;
  private static final int MAJOR_CATEGORY_SHADOW_COLOR = 0x57000000;

  // TODO(hidehiko): This parameter is not fixed yet. Needs to revisit again.
  private static final float SYMBOL_VIEW_MINOR_CATEGORY_TAB_SELECTED_HEIGHT = 6f;

  private static final int NUM_TABS = 6;

  private SymbolCandidateStorage symbolCandidateStorage;

  @VisibleForTesting SymbolMajorCategory currentMajorCategory;
  @VisibleForTesting boolean emojiEnabled;
  private boolean isPasswordField;
  @VisibleForTesting EmojiProviderType emojiProviderType = EmojiProviderType.NONE;

  @VisibleForTesting SharedPreferences sharedPreferences;
  @VisibleForTesting AlertDialog emojiProviderDialog;

  private ViewEventListener viewEventListener;
  private final KeyEventButtonTouchListener deleteKeyEventButtonTouchListener =
      createDeleteKeyEventButtonTouchListener(getResources());
  private OnClickListener closeButtonClickListener = null;
  private final SymbolCandidateSelectListener symbolCandidateSelectListener =
      new SymbolCandidateSelectListener();

  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;
  private final MozcDrawableFactory mozcDrawableFactory = new MozcDrawableFactory(getResources());
  private final SymbolMajorCategoryButtonDrawableFactory majorCategoryButtonDrawableFactory =
      new SymbolMajorCategoryButtonDrawableFactory(
          mozcDrawableFactory,
          MAJOR_CATEGORY_TOP_COLOR,
          MAJOR_CATEGORY_BOTTOM_COLOR,
          MAJOR_CATEGORY_PRESSED_TOP_COLOR,
          MAJOR_CATEGORY_PRESSED_BOTTOM_COLOR,
          MAJOR_CATEGORY_SHADOW_COLOR,
          BUTTON_CORNOR_RADIUS * getResources().getDisplayMetrics().density);
  // Candidate text size in dip.
  private float candidateTextSize;
  // Description text size in dip.
  private float desciptionTextSize;

  public SymbolInputView(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
  }

  public SymbolInputView(Context context) {
    super(context);
  }

  public SymbolInputView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  {
    setOutAnimationListener(new OutAnimationAdapter());
    sharedPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());
  }

  private static KeyEventButtonTouchListener createDeleteKeyEventButtonTouchListener(
      Resources resources) {
    // Use 8 as default value of backspace key code (only for testing).
    // This code is introduced just for testing purpose due to AndroidMock's limitation.
    // When we move to EasyMock, we can remove this method.
    int ucharBackspace = resources == null ? 8 : resources.getInteger(R.integer.uchar_backspace);
    return new KeyEventButtonTouchListener(DELETE_BUTTON_SOURCE_ID, ucharBackspace);
  }


  boolean isInflated() {
    return getChildCount() > 0;
  }

  void inflateSelf() {
    if (isInflated()) {
      throw new IllegalStateException("The symbol input view is already inflated.");
    }

    // Hack: Because we wrap the real context to inject "retrying" for Drawable loading,
    // LayoutInflater.from(getContext()).getContext() may be different from getContext().
    // So, we clone the inflater here, too, with actual context.
    Context context = getContext();
    LayoutInflater inflater = LayoutInflater.from(context);
    inflater = inflater.cloneInContext(context);
    MozcUtil.inflateWithOutOfMemoryRetrial(
        SymbolInputView.class, inflater, R.layout.symbol_view, Optional.<ViewGroup>of(this), true);
    // Note: onFinishInflate won't be invoked on android ver 3.0 or later, while it is invoked
    // on android 2.3 or earlier. So, we define another (but similar) method and invoke it here
    // manually.
    onFinishInflateSelf();
  }

  /**
   * Initializes the instance. Called only once.
   * {@code onFinishInflate()} is *not* invoked for the inflation of &lt;merge&gt; element we use.
   * So, instead, we define another onFinishInflate method and invoke this manually.
   */
  protected void onFinishInflateSelf() {
    initializeMajorCategoryButtons();
    initializeMinorCategoryTab();
    initializeCloseButton();
    initializeDeleteButton();

    resetMajorCategoryBackground();
    resetTabBackground();
    enableEmoji(emojiEnabled);
    reset();
  }

  private void resetCandidateViewPager() {
    if (!isInflated()) {
      return;
    }

    ViewPager candidateViewPager = getCandidateViewPager();
    TabHost tabHost = getTabHost();

    SymbolTabWidgetViewPagerAdapter adapter = new SymbolTabWidgetViewPagerAdapter(
        getContext(),
        symbolCandidateStorage, viewEventListener, symbolCandidateSelectListener,
        currentMajorCategory, skinType, emojiProviderType, tabHost, candidateViewPager,
        candidateTextSize, desciptionTextSize);
    candidateViewPager.setAdapter(adapter);
    candidateViewPager.setOnPageChangeListener(adapter);
    tabHost.setOnTabChangedListener(adapter);
  }

  private void resetMajorCategoryBackground() {
    View view = findViewById(R.id.symbol_major_category);
    if (view != null) {
      if (skinType == null) {
        view.setBackgroundColor(Color.BLACK);
      } else {
        view.setBackgroundResource(skinType.windowBackgroundResourceId);
      }
    }
  }

  private void setMozcDrawable(ImageView imageView, int resourceId) {
    Optional<Drawable> drawable = mozcDrawableFactory.getDrawable(resourceId);
    if (drawable.isPresent()) {
      imageView.setImageDrawable(drawable.get());
    }
  }

  /**
   * Sets click event handlers to each major category button.
   * It is necessary that the inflation has been done before this method invocation.
   */
  @SuppressWarnings("deprecation")
  private void initializeMajorCategoryButtons() {
    for (SymbolMajorCategory majorCategory : SymbolMajorCategory.values()) {
      ImageView view = ImageView.class.cast(findViewById(majorCategory.buttonResourceId));
      if (view == null) {
        throw new IllegalStateException(
            "The view corresponding to " + majorCategory.name() + " is not found.");
      }
      view.setOnClickListener(new MajorCategoryButtonClickListener(majorCategory));
      setMozcDrawable(view, majorCategory.buttonImageResourceId);

      switch (majorCategory) {
        case SYMBOL:
          view.setBackgroundDrawable(
              majorCategoryButtonDrawableFactory.createLeftButtonDrawable());
          break;
        case EMOJI:
          view.setBackgroundDrawable(
              majorCategoryButtonDrawableFactory.createRightButtonDrawable(emojiEnabled));
          break;
        default:
          view.setBackgroundDrawable(
              majorCategoryButtonDrawableFactory.createCenterButtonDrawable());
          break;
      }
    }
  }

  private void initializeMinorCategoryTab() {
    TabHost tabhost = TabHost.class.cast(findViewById(android.R.id.tabhost));
    tabhost.setup();

    float textSize = getResources().getDimension(R.dimen.symbol_view_minor_category_text_size);
    int textColor = getResources().getColor(android.R.color.black);

    // Create NUM_TABS (= 6) tabs.
    // Note that we may want to change the number of tabs, however due to the limitation of
    // the current TabHost implementation, it is difficult. Fortunately, all major categories
    // have the same number of minor categories, so we use it as hard-coded value.
    for (int i = 0; i < NUM_TABS; ++i) {
      // The tab's id is the index of the tab.
      TabSpec tab = tabhost.newTabSpec(String.valueOf(i));
      TextView textView = new TabTextView(getContext());
      textView.setTypeface(Typeface.DEFAULT_BOLD);
      textView.setTextColor(textColor);
      textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSize);
      textView.setGravity(Gravity.CENTER);
      tab.setIndicator(textView);
      // Set dummy view for the content. The actual content will be managed by ViewPager.
      tab.setContent(R.id.symbol_input_dummy);
      tabhost.addTab(tab);
    }

    // Hack: Set the current tab to the non-default (neither 0 nor 1) position,
    // so that the reset process will set the view's visibility appropriately.
    tabhost.setCurrentTab(2);
  }

  @SuppressWarnings("deprecation")
  private void resetTabBackground() {
    if (!isInflated()) {
      return;
    }

    float density = getResources().getDisplayMetrics().density;

    TabWidget tabWidget = TabWidget.class.cast(findViewById(android.R.id.tabs));
    for (int i = 0; i < tabWidget.getTabCount(); ++i) {
      View view = tabWidget.getChildTabViewAt(i);
      view.setBackgroundDrawable(createTabBackgroundDrawable(skinType, density));
    }
  }

  private static Drawable createTabBackgroundDrawable(SkinType skinType, float density) {
    return new LayerDrawable(new Drawable[] {
        BackgroundDrawableFactory.createSelectableDrawable(
            new TabSelectedBackgroundDrawable(
                (int) (SYMBOL_VIEW_MINOR_CATEGORY_TAB_SELECTED_HEIGHT * density),
                skinType.symbolMinorCategoryTabSelectedColor),
            null),
        BackgroundDrawableFactory.createPressableDrawable(
            new ColorDrawable(skinType.symbolMinorCategoryTabPressedColor), null),
    });
  }

  private void resetTabText() {
    if (!isInflated()) {
      return;
    }

    TabWidget tabWidget = TabWidget.class.cast(findViewById(android.R.id.tabs));
    List<SymbolMinorCategory> minorCategoryList = currentMajorCategory.minorCategories;
    for (int i = 0; i < tabWidget.getChildCount(); ++i) {
      TextView textView = TextView.class.cast(tabWidget.getChildTabViewAt(i));
      textView.setText(minorCategoryList.get(i).textResourceId);
    }
  }

  private static Drawable createButtonBackgroundDrawable(SkinType skinType, float density) {
    return BackgroundDrawableFactory.createPressableDrawable(
        new RoundRectKeyDrawable(
            (int) (BUTTON_LEFT_OFFSET * density),
            (int) (BUTTON_TOP_OFFSET * density),
            (int) (BUTTON_RIGHT_OFFSET * density),
            (int) (BUTTON_BOTTOM_OFFSET * density),
            (int) (BUTTON_CORNOR_RADIUS * density),
            skinType.symbolPressedFunctionKeyTopColor,
            skinType.symbolPressedFunctionKeyBottomColor,
            skinType.symbolPressedFunctionKeyHighlightColor,
            skinType.symbolPressedFunctionKeyShadowColor),
        new RoundRectKeyDrawable(
            (int) (BUTTON_LEFT_OFFSET * density),
            (int) (BUTTON_TOP_OFFSET * density),
            (int) (BUTTON_RIGHT_OFFSET * density),
            (int) (BUTTON_BOTTOM_OFFSET * density),
            (int) (BUTTON_CORNOR_RADIUS * density),
            skinType.symbolReleasedFunctionKeyTopColor,
            skinType.symbolReleasedFunctionKeyBottomColor,
            skinType.symbolReleasedFunctionKeyHighlightColor,
            skinType.symbolReleasedFunctionKeyShadowColor));
  }

  @SuppressWarnings("deprecation")
  private void initializeCloseButton() {
    ImageView closeButton = ImageView.class.cast(findViewById(R.id.symbol_view_close_button));
    if (closeButtonClickListener != null) {
      closeButton.setOnClickListener(closeButtonClickListener);
    }
    closeButton.setBackgroundDrawable(
        createButtonBackgroundDrawable(skinType, getResources().getDisplayMetrics().density));
    setMozcDrawable(closeButton, R.raw.symbol__function__close);
    closeButton.setPadding(2, 2, 2, 2);
  }

  /**
   * Sets a click event handler to the delete button.
   * It is necessary that the inflation has been done before this method invocation.
   */
  @SuppressWarnings("deprecation")
  private void initializeDeleteButton() {
    ImageView deleteButton = ImageView.class.cast(findViewById(R.id.symbol_view_delete_button));
    deleteButton.setOnTouchListener(deleteKeyEventButtonTouchListener);
    deleteButton.setBackgroundDrawable(
        createButtonBackgroundDrawable(skinType, getResources().getDisplayMetrics().density));
    setMozcDrawable(deleteButton, R.raw.symbol__function__delete);
    deleteButton.setPadding(0, 0, 0, 0);
  }

  TabHost getTabHost() {
    return TabHost.class.cast(findViewById(android.R.id.tabhost));
  }

  ViewPager getCandidateViewPager() {
    return ViewPager.class.cast(findViewById(R.id.symbol_input_candidate_view_pager));
  }

  ImageButton getMajorCategoryButton(SymbolMajorCategory majorCategory) {
    if (majorCategory == null) {
      throw new NullPointerException("majorCategory shouldn't be null.");
    }
    return ImageButton.class.cast(findViewById(majorCategory.buttonResourceId));
  }

  View getEmojiDisabledMessageView() {
    return findViewById(R.id.symbol_emoji_disabled_message_view);
  }

  public void setEmojiEnabled(boolean unicodeEmojiEnabled, boolean carrierEmojiEnabled) {
    this.emojiEnabled = unicodeEmojiEnabled || carrierEmojiEnabled;
    enableEmoji(this.emojiEnabled);
    symbolCandidateStorage.setEmojiEnabled(unicodeEmojiEnabled, carrierEmojiEnabled);
  }

  public void setPasswordField(boolean isPasswordField) {
    this.isPasswordField = isPasswordField;
  }

  @SuppressWarnings("deprecation")
  private void enableEmoji(boolean enableEmoji) {
    if (!isInflated()) {
      return;
    }

    ImageButton imageButton = getMajorCategoryButton(SymbolMajorCategory.EMOJI);
    imageButton.setBackgroundDrawable(
        majorCategoryButtonDrawableFactory.createRightButtonDrawable(enableEmoji));
  }

  /**
   * Resets the status.
   */
  void reset() {
    // the current minor category is also updated in setMajorCategory.
    setMajorCategory(SymbolMajorCategory.SYMBOL);
    deleteKeyEventButtonTouchListener.reset();
  }

  @Override
  public void setVisibility(int visibility) {
    int previousVisibility = getVisibility();
    super.setVisibility(visibility);
    if (viewEventListener != null
        && previousVisibility == View.VISIBLE && visibility != View.VISIBLE) {
      viewEventListener.onCloseSymbolInputView();
    }
  }

  void setSymbolCandidateStorage(SymbolCandidateStorage symbolCandidateStorage) {
    this.symbolCandidateStorage = symbolCandidateStorage;
  }

  void setKeyEventHandler(KeyEventHandler keyEventHandler) {
    deleteKeyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);
  }

  void setCandidateTextDimension(float candidateTextSize, float descriptionTextSize) {
    Preconditions.checkArgument(candidateTextSize > 0);
    Preconditions.checkArgument(descriptionTextSize > 0);

    this.candidateTextSize = candidateTextSize;
    this.desciptionTextSize = descriptionTextSize;
  }

  /**
   * Initializes EmojiProvider selection dialog, if necessary.
   * Exposed as protected for testing purpose.
   */
  protected void maybeInitializeEmojiProviderDialog(Context context) {
    if (emojiProviderDialog != null) {
      return;
    }

    EmojiProviderDialogListener listener = new EmojiProviderDialogListener(context);
    AlertDialog dialog = new AlertDialog.Builder(context)
        .setTitle(R.string.pref_emoji_provider_type_title)
        .setItems(R.array.pref_emoji_provider_type_entries, listener)
        .create();
    this.emojiProviderDialog = dialog;
  }

  /**
   * Sets the major category to show.
   *
   * The view is updated.
   * The active minor category is also updated.
   *
   * @param newCategory the major category to show.
   */
  protected void setMajorCategory(SymbolMajorCategory newCategory) {
    if (newCategory == null) {
      throw new NullPointerException("newCategory must be non-null.");
    }
    currentMajorCategory = newCategory;

    // Reset the minor category to the default value.
    resetTabText();
    resetCandidateViewPager();
    SymbolMinorCategory minorCategory = currentMajorCategory.getDefaultMinorCategory();
    if (symbolCandidateStorage.getCandidateList(minorCategory).getCandidatesCount() == 0) {
      minorCategory = currentMajorCategory.getMinorCategoryByRelativeIndex(minorCategory, 1);
    }
    int index = newCategory.minorCategories.indexOf(minorCategory);
    getCandidateViewPager().setCurrentItem(index);
    getTabHost().setCurrentTab(index);

    // Update visibility relating attributes.
    for (SymbolMajorCategory majorCategory : SymbolMajorCategory.values()) {
      // Update major category selector button's look and feel.
      ImageButton button = getMajorCategoryButton(majorCategory);
      if (button != null) {
        button.setSelected(majorCategory == newCategory);
        button.setEnabled(majorCategory != newCategory);
      }
    }

    View emojiDisabledMessageView = getEmojiDisabledMessageView();
    if (emojiDisabledMessageView != null) {
      // Show messages about emoji-disabling, if necessary.
      emojiDisabledMessageView.setVisibility(
          newCategory == SymbolMajorCategory.EMOJI && !emojiEnabled ? View.VISIBLE : View.GONE);
    }
  }

  void setEmojiProviderType(EmojiProviderType emojiProviderType) {
    Preconditions.checkNotNull(emojiProviderType);

    this.emojiProviderType = emojiProviderType;
    this.symbolCandidateStorage.setEmojiProviderType(emojiProviderType);
    if (!isInflated()) {
      return;
    }

    resetCandidateViewPager();
  }

  void setViewEventListener(ViewEventListener listener, OnClickListener closeButtonClickListener) {
    if (listener == null) {
      throw new NullPointerException("lister must be non-null.");
    }
    viewEventListener = listener;
    this.closeButtonClickListener = closeButtonClickListener;
  }

  void setSkinType(SkinType skinType) {
    if (this.skinType == skinType) {
      return;
    }

    this.skinType = skinType;
    mozcDrawableFactory.setSkinType(skinType);
    if (!isInflated()) {
      return;
    }

    // Reset the minor category tab, candidate view and major category buttons.
    resetTabBackground();
    resetCandidateViewPager();
    resetMajorCategoryBackground();
  }

  @Override
  public void trimMemory() {
    ViewGroup viewGroup = getCandidateViewPager();
    if (viewGroup == null) {
      return;
    }
    for (int i = 0; i < viewGroup.getChildCount(); ++i) {
      View view = viewGroup.getChildAt(i);
      if (view instanceof MemoryManageable) {
        MemoryManageable.class.cast(view).trimMemory();
      }
    }
  }
}
